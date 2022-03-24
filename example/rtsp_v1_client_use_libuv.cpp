#include <memory>
#include <iostream>
#include <vector>
#include <list>
#include <tuple>
#include <thread>
#include <cstring>
#include "Rtsp2Client.h"
#include "uv.h"

using namespace std::placeholders;

class RtspClient
{
public:

    static void tcp_alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
    {
        auto client = (RtspClient*)handle->data;
        buf->base = client->cache;
        buf->len = sizeof(client->cache);
    }

    static void on_tcp_read(uv_stream_t * handle, ssize_t nread, const uv_buf_t *buf)
    {
        auto client = (RtspClient*)handle->data;
        if (nread < 0)
        {
            if (nread != UV_EOF)
                std::cout<< "Read error " <<  uv_err_name(nread) <<std::endl;
            client->stop();
            return;
        }
        std::cout<<"read some byte" << nread<<std::endl;
        if(client->rtspProtocolClient.input(buf->base,nread)) 
        {
            std::cout<<"rtsp failed" <<std::endl;
            exit(0);
        }

        if (nread >= 0)
        {
            uv_read_start((uv_stream_t *)&client->sock, tcp_alloc_buffer, on_tcp_read);
        }
    }

    static void on_tcp_connect(uv_connect_t *req, int status)
    {
        auto client = (RtspClient*)req->data;
        if(status < 0)
        {
            client->stop();
            return;
        }
        std::cout<<"connect sucessful"<<std::endl;
        uv_read_start((uv_stream_t *)&client->sock, tcp_alloc_buffer, on_tcp_read);
        client->rtspProtocolClient.options();
    }

    static void on_tcp_write(uv_write_t* req,int status)
    {
        auto client = (RtspClient*)req->data;
        if(status < 0)
        {
            client->stop();
            return;
        }
        client->sendlist.pop_front();
        if(client->sendlist.empty())
        {
            return;
        }
        std::cout<<"on tcp write"<<std::endl;
        uv_buf_t vec;
        vec.base = (char*)client->sendlist.front().c_str();
        vec.len = client->sendlist.front().size();
        uv_write(&client->wh, (uv_stream_t *)&client->sock, &vec, 1, on_tcp_write);
    }

public:
    RtspClient(const std::string& url)
        :rtspProtocolClient(url)
    {
        connect_req.data = this;
        sock.data = this;
        memset(&wh,0,sizeof(wh));
        wh.data = this;
       
        uv_tcp_init(uv_default_loop(), &sock);
        rtspProtocolClient.setOptionsCB(std::bind(&RtspClient::OnOption,this,_1,_2));
        rtspProtocolClient.setDescribeCB(std::bind(&RtspClient::OnDescribe,this,_1,_2,_3));  
        rtspProtocolClient.setSetupCB(std::bind(&RtspClient::OnSetUp,this,_1,_2,_3,_4,_5)); 
        rtspProtocolClient.setPlayCB(std::bind(&RtspClient::OnPlay,this,_1,_2));
        rtspProtocolClient.setTearDownCB(std::bind(&RtspClient::OnTearDown,this,_1,_2));
        rtspProtocolClient.setRtpCB(std::bind(&RtspClient::OnRtp,this,_1,_2,_3,_4,_5));
        rtspProtocolClient.setOutputCB(std::bind(&RtspClient::OnOutput,this,_1));
        rtspProtocolClient.setAuthFailedCB(std::bind(&RtspClient::OnAuthFailed,this,_1,_2,_3));
        rtspProtocolClient.setRequestCB(std::bind(&RtspClient::OnRequest,this,_1,_2));
    }

    void start()
    {   
        uint16_t port = 554;
        if(!rtspProtocolClient.url().port.empty())
        {
            port = std::stoi(rtspProtocolClient.url().port);
        }
        sockaddr_in remote;
        uv_inet_pton(AF_INET,rtspProtocolClient.url().host.c_str(),&remote.sin_addr);
        remote.sin_family = AF_INET;
        remote.sin_port = htons(port);
        uv_tcp_connect(&connect_req, &sock, (const struct sockaddr *)&remote, on_tcp_connect);
    }

    void stop()
    {
        if(uv_is_closing((uv_handle_t*)&sock))
        {
            return;
        }
        uv_close((uv_handle_t*)&sock,[](uv_handle_t* handle){
                    auto client = (RtspClient*)handle->data;
                    delete client;
                    std::cout<<"destory client"<<std::endl;
            });
    }

private:

    void OnOption(rtsp2::Client& client, const rtsp2::RtspResponse & res)
    {
        std::cout<<"recv options: \n"
                << res.toString();
        client.describe();
    }

    void OnDescribe(rtsp2::Client& client, const rtsp2::RtspResponse & res,const rtsp2::Sdp& sdp)
    {
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
        client.setup(trans);
    }

    void OnSetUp(rtsp2::Client& client, const rtsp2::RtspResponse & res,const rtsp2::RtspSessionId& session,const std::string& media, const rtsp2::TransportV1& trans)
    {
        std::cout<<"recv setup reply :\n"
                << res.toString();

        if(res.statusCode() != rtsp2::RtspResponse::RTSP_OK)
        {
            std::cout<<"unsupport transport type :\n";
            stop();
            return ;
        }

        sessionId = session.sessionid;
        timeout = session.timeout;
        std::cout<<"session id:" <<sessionId << " timeout " << timeout<<std::endl;
        setUpcount++;
        if(setUpcount == sdp_.mediaDescriptions.size())
        {
            client.play();
        }
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
        std::cout<<"send rtspmessge:\n"
                <<msg<<std::endl;
        sendlist.push_back(msg);
        if(uv_is_active((uv_handle_t*)&wh))
        {
            std::cout<<"wh is active" <<std::endl;
            return;
        }
        std::cout<<"uv write "<<std::endl;
        uv_buf_t vec;
        vec.base = (char*)sendlist.front().c_str();
        vec.len = sendlist.front().size();
        uv_write(&wh, (uv_stream_t *)&sock, &vec, 1, on_tcp_write);
    }


private:
    uv_connect_t connect_req;
    uv_tcp_t sock;
    uv_write_t wh;
    char cache[4096];
    std::list<std::string> sendlist;
    uint32_t setUpcount = 0;


    std::string sessionId = "";
    int timeout = 0;
    rtsp2::Sdp sdp_;
    rtsp2::Client rtspProtocolClient;
};

int main(int argc,char* argv[])
{
    signal(SIGPIPE,SIG_IGN);
    auto cli = new RtspClient(argv[1]);
    cli->start();
    uv_run(uv_default_loop(),UV_RUN_DEFAULT);
}