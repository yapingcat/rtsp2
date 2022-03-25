#include "asio.hpp"
#include "Rtsp2Client.h"
#include "h264meidasource.h"
#include "rtp_h264_packer.h"

#include <string>
#include <iostream>
#include <cstdint>
#include <vector>
#include <list>

using namespace std::placeholders;


class AsioRtspClient
{
public:
    AsioRtspClient(asio::io_context& io_context,const std::string& url)
        :ctx(io_context)
        ,sock(ctx)
        ,rtsp2Client(url)
    {
        cache.resize(4096);
        asio::ip::tcp::resolver resolver(ctx);
        auto endpoints = resolver.resolve(rtsp2Client.url().host, rtsp2Client.url().port);
        do_connect(endpoints);
        rtsp2Client.setOptionsCB(std::bind(&AsioRtspClient::OnOption,this,_1,_2));
        rtsp2Client.setDescribeCB(std::bind(&AsioRtspClient::OnDescribe,this,_1,_2,_3));  
        rtsp2Client.setSetupCB(std::bind(&AsioRtspClient::OnSetUp,this,_1,_2,_3,_4,_5)); 
        rtsp2Client.setPlayCB(std::bind(&AsioRtspClient::OnPlay,this,_1,_2));
        rtsp2Client.setTearDownCB(std::bind(&AsioRtspClient::OnTearDown,this,_1,_2));
        rtsp2Client.setRtpCB(std::bind(&AsioRtspClient::OnRtp,this,_1,_2,_3,_4,_5));
        rtsp2Client.setOutputCB(std::bind(&AsioRtspClient::OnOutput,this,_1));
        rtsp2Client.setAuthFailedCB(std::bind(&AsioRtspClient::OnAuthFailed,this,_1,_2,_3));
        rtsp2Client.setRequestCB(std::bind(&AsioRtspClient::OnRequest,this,_1,_2));
    }
    
public:

    void stop()
    {
        std::cout<<"close client"<<std::endl;
        sock.close();
    }
    
private:

    void do_connect(const asio::ip::tcp::resolver::results_type& endpoints)
    {
        asio::async_connect(sock, endpoints,[this](std::error_code ec, asio::ip::tcp::endpoint)
        {
            if (!ec)
            {
                rtsp2Client.options();
                do_read();
                return;
            }
            stop();
        });
    }

    void do_read()
    {
        sock.async_read_some(asio::buffer(cache),[this](asio::error_code ec,std::size_t nread) {
                if(!ec)
                {
                    if(rtsp2Client.input((const char*)cache.data(),nread))
                    {
                        std::cout<<"input failed" <<std::endl;
                        stop();
                        return;
                    }
                    do_read();
                }
                else if(ec != asio::error::operation_aborted)
                {
                    std::cout<<"read error " << ec.message() <<std::endl;
                    stop();
                }
        });
    }

    void do_write()
    {
        asio::async_write(sock,asio::buffer(sendLists_.front()),[this](std::error_code ec, std::size_t /*length*/){
            if(!ec)
            {
                sendLists_.pop_front();
                if(sendLists_.empty())
                {
                    return;
                }
                do_write();
            }
            else if(ec != asio::error::operation_aborted)
            {
                std::cout<<"write error"<<ec.message()<<std::endl;
                stop();
            }
            
        }); 
    }

    void OnOption(rtsp2::Client& client, const rtsp2::RtspResponse & res)
    {
        std::cout<<"recv option response\n"
                 << res.toString();
        client.describe();
    }

    void OnDescribe(rtsp2::Client& client, const rtsp2::RtspResponse & res,const rtsp2::Sdp& sdp)
    {
        std::cout<<"recv describe response\n"
                 << res.toString();
        std::vector<std::tuple<std::string,rtsp2::TransportV1>> trans;
        sdp_ = sdp;
        for(auto && md : sdp.mediaDescriptions)
        {
            rtsp2::TransportV1 transport;
            transport.cast = rtsp2::UNICAST;
            transport.mode = rtsp2::PLAY;
            transport.proto = rtsp2::TCP;
            if(md.media.media == "video")
            {
                transport.tcpParam.interleaved[0] = 0;
                transport.tcpParam.interleaved[1] = 1;
            }
            else if(md.media.media == "audio")
            {
                transport.tcpParam.interleaved[0] = 2;
                transport.tcpParam.interleaved[1] = 3;
            }
            trans.push_back(std::make_tuple(md.media.media,transport));
        }
        client.startPipeline();
        client.setup(trans);
        client.play();
        client.stopPipeline();
    }

    void OnSetUp(rtsp2::Client& client, const rtsp2::RtspResponse & res,const rtsp2::RtspSessionId& session,const std::string& media, const rtsp2::TransportV1& trans)
    {
        std::cout<<"recv setup reply :\n"
                << res.toString();
        std::cout<<media <<"transport \n"
                 << trans.toString();
        if(res.statusCode() != rtsp2::RtspResponse::RTSP_OK)
        {
            std::cout<<"unsupport transport type :\n";
            stop();
            return ;
        }
        sessionId = session.sessionid;
        timeout = session.timeout;
    }

    void OnPlay(rtsp2::Client& client, const rtsp2::RtspResponse & res)
    {
        std::cout<<"recv play reply:\n"
                << res.toString();
    }

    void OnTearDown(rtsp2::Client& client, const rtsp2::RtspResponse & res)
    {
        std::cout<<"recv teardown reply:\n"
                << res.toString();
    }


    void OnRtp(rtsp2::Client& client,const std::string &media, int isRtcp, const uint8_t *, std::size_t len)
    {
        std::cout<<"recv: " << media <<"  "<< (isRtcp ? "rtcp" : "rtp") << "  packet len:" <<len<< std::endl; 
    }

    void OnAuthFailed(rtsp2::Client& client,const std::string &username,const std::string& pwd)
    {
        std::cout<<"auth failed: " << username <<" "<< pwd << std::endl; 
    }

    void OnRequest(const rtsp2::RtspRequest& req,rtsp2::RtspResponse& res)
    {
        std::cout<<"recv rtsp request\n"
                << req.toString();
    }

    void OnOutput(const std::string & msg)
    {
        if(!startRecord_)
        {
            std::cout<<"send request\n"
                     <<msg<<std::endl;
        }
        bool isInWriteProcess = !sendLists_.empty();
        sendLists_.push_back(msg);
        if(isInWriteProcess)
        {
            return;
        }
        do_write();
    }

private:
    asio::io_context& ctx;
    asio::ip::tcp::socket sock;
    std::string sessionId = "";
    std::vector<uint8_t> cache;
    int timeout = 0;
    rtsp2::Sdp sdp_;
    rtsp2::Client rtsp2Client;
    std::list<std::string> sendLists_;
    int  startRecord_ = 0;
};

int main(int argc,char* argv[])
{
    asio::io_context io_context;
    AsioRtspClient client(io_context,argv[1]);
    io_context.run();
}


