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
    AsioRtspClient(asio::io_context& io_context,const std::string& url,const std::string& filename)
        :ctx(io_context)
        ,sock(ctx)
        ,rtsp2Client(url)
        ,sendFrameTimer_(ctx)
        ,source_(filename)
        ,packer_(1000,96,0x12345678)
    {
        packer_.setRtpCallBack([this](const uint8_t* pkg,std::size_t length) {
            rtsp2Client.pushInterleavedBinaryData(videoTransport_.tcpParam.interleaved[0],pkg,length);
        });
        cache.resize(4096);
        asio::ip::tcp::resolver resolver(ctx);
        auto endpoints = resolver.resolve(rtsp2Client.url().host, rtsp2Client.url().port);
        do_connect(endpoints);
        rtsp2Client.setOptionsCB(std::bind(&AsioRtspClient::OnOption,this,_1,_2));
        rtsp2Client.setAnnounceCB(std::bind(&AsioRtspClient::OnAnnounce,this,_1,_2));  
        rtsp2Client.setSetupCB(std::bind(&AsioRtspClient::OnSetUp,this,_1,_2,_3,_4,_5)); 
        rtsp2Client.setRecordCB(std::bind(&AsioRtspClient::OnRecord,this,_1,_2));
        rtsp2Client.setTearDownCB(std::bind(&AsioRtspClient::OnTearDown,this,_1,_2));
        rtsp2Client.setRtpCB(std::bind(&AsioRtspClient::OnRtp,this,_1,_2,_3,_4,_5));
        rtsp2Client.setOutputCB(std::bind(&AsioRtspClient::OnOutput,this,_1));
        rtsp2Client.setAuthFailedCB(std::bind(&AsioRtspClient::OnAuthFailed,this,_1,_2,_3));
        
    }
    
public:

    void stop()
    {
        std::cout<<"close client"<<std::endl;
        sock.close();
        sendFrameTimer_.cancel();
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
                    if(rtsp2Client.input((const char*)cache.data(),nread) != 0)
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

    void onTimeout()
    {
        char *frame;
        int len;       
        std::tie(frame,len) = source_.GetNextFrame();
        if(frame == nullptr)
        {
            stop();
            return;
        }
        packer_.pack((uint8_t*)frame,len,timestamp_ * 90);
        timestamp_ += 40;
        sendFrameTimer_.expires_after(std::chrono::milliseconds(40));
        sendFrameTimer_.async_wait(std::bind(&AsioRtspClient::onTimeout,this));
    }

    void OnOption(rtsp2::Client& client, const rtsp2::RtspResponse & res)
    {
        std::cout<<"recv options: \n"
                << res.toString();
        std::string sdp = "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\na=control:*\r\n"
                          "m=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/90000\r\na=control:trackID=0\r\n";   
        client.announce(sdp);
    }

    void OnAnnounce(rtsp2::Client& client, const rtsp2::RtspResponse & res)
    {
        std::cout<<"recv Annouce reply: \n"
                 << res.toString();
        videoTransport_.proto = rtsp2::TCP;
        videoTransport_.cast = rtsp2::UNICAST;
        videoTransport_.tcpParam.interleaved[0] = 0;
        videoTransport_.tcpParam.interleaved[1] = 1;
        client.setup("video",videoTransport_);
    }

    void OnSetUp(rtsp2::Client& client, const rtsp2::RtspResponse & res,const rtsp2::RtspSessionId& session,const std::string& media, const rtsp2::TransportV1& trans)
    {
        std::cout<<"recv setup reply :\n"
                << res.toString();

        if(res.statusCode() != rtsp2::RtspResponse::RTSP_OK)
        {
            std::cout << "unsupport transport type :\n";
            //stop();
            return ;
        }

        sessionId = session.sessionid;
        timeout = session.timeout;
        std::cout<<"session id:" <<sessionId << " timeout " << timeout<<std::endl;
        client.record();
    }

    void OnRecord(rtsp2::Client& client, const rtsp2::RtspResponse & res)
    {
        std::cout<<"recv record reply:\n"
                << res.toString();
        startRecord_ = 1;
        sendFrameTimer_.expires_after(std::chrono::milliseconds(40));
        sendFrameTimer_.async_wait(std::bind(&AsioRtspClient::onTimeout,this));
    }

    void OnTearDown(rtsp2::Client& client, const rtsp2::RtspResponse & res)
    {
        std::cout<<"recv teardown reply:\n"
                << res.toString();
    }


    void OnRtp(rtsp2::Client& client,const std::string &media, int isRtcp, const uint8_t *, std::size_t len)
    {
       // std::cout<<"recv: " << media <<"  "<< (isRtcp ? "rtcp" : "rtp") << "  packet len:" <<len<< std::endl; 
    }

    void OnAuthFailed(rtsp2::Client& client,const std::string &username,const std::string& pwd)
    {
        std::cout<<"auth failed: " << username <<" "<< pwd << std::endl; 
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
    std::string filename;
    asio::steady_timer sendFrameTimer_;
    H264MeidaSource source_;
    H264RtpPacker packer_;
    rtsp2::TransportV1 videoTransport_;
    uint32_t timestamp_ = 0;
    int  startRecord_ = 0;
};

int main(int argc,char* argv[])
{
    asio::io_context io_context;
    AsioRtspClient client(io_context,argv[1],argv[2]);
    io_context.run();
}


