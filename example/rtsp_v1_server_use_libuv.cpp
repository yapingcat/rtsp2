#include "Rtsp2Server.h"
#include "h264meidasource.h"
#include "rtp_h264_packer.h"
#include "uv.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <deque>
#include <sstream>
#include <unordered_map>

using namespace rtsp2;

std::string GFileName = "";
class Session;

class Session : public rtsp2::ServerHandle
{
public:

    static void onAllocate(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
    {
        auto impl = (Session *)handle->data;
        buf->base = impl->buf;
        buf->len = 65535;
    }

    static void doRead(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
    {
        auto impl = (Session *)client->data;
        if (nread < 0)
        {
            std::cout<<"read failed " << nread <<std::endl;
            impl->close();    
        }
        if(nread > 0)
        {
            impl->input(buf->base,nread);
        }
        uv_read_start((uv_stream_t *)impl->sess, onAllocate, doRead);
        return;
    }

    static void on_tcp_write(uv_write_t *req, int status)
    {
        auto sess = (Session*)req->data;
        if(status < 0)
        {
            std::cout<<"write failed " << status <<std::endl;
            sess->close();    
        }
        {
            std::lock_guard<std::mutex> guard(sess->mtx);
            sess->sendList.pop_front();
            if(sess->sendList.empty())
            {
                sess->writeInProgress = false;
                return;
            }
            uv_buf_t tmp = uv_buf_init((char*)sess->sendList.front().data(),sess->sendList.front().size());
            uv_write(&sess->wh, (uv_stream_t *)sess->sess, &tmp, 1, on_tcp_write);
        }
    }

    static void start_tcp_write(uv_async_t *handle)
    {
        auto sess = (Session*)handle->data;
        {
            std::lock_guard<std::mutex> guard(sess->mtx);
            if(sess->sendList.empty() || sess->writeInProgress)
            {
                return;
            }
            sess->writeInProgress = true;
            sess->uvbuf = uv_buf_init((char*)sess->sendList.front().data(),sess->sendList.front().size());
            uv_write(&sess->wh, (uv_stream_t *)sess->sess, &sess->uvbuf, 1, on_tcp_write);
        }
    }

    static void close_tcp(uv_async_t *handle)
    {
        auto impl = (Session *)handle->data;
        uv_close((uv_handle_t*)impl->sess,[](uv_handle_t* handle){
                    auto rtspsess = (Session*)handle->data;
                    rtspsess->teardown = 1;
                    rtspsess->startPlay = 0;
                    if(rtspsess->playThread.joinable())
                        rtspsess->playThread.join();
                    delete rtspsess;
            });
    }


public:
    Session(uv_tcp_t *sess)
        :sess(sess)
    {
        writeInProgress = false;
        sess->data = this;
        wh.data = this;
        write_async.data = this;
        close_async.data = this;
        uv_async_init(uv_default_loop(), &write_async, start_tcp_write);
        uv_async_init(uv_default_loop(), &close_async, close_tcp);
    }

    ~Session()
    {
        delete sess;
    }
public:

    void start()
    {
        uv_read_start((uv_stream_t *)sess, onAllocate, doRead);
    }

    void close()
    {
        std::call_once(flag,[this](){
            uv_async_send(&close_async);
        });
    }

    int handleOption(const RtspRequest&req,RtspResponse& res)
    {
        std::cout<<"recv request:\n"
                 <<req.toString();
        res[RtspMessage::Public] = RtspMessage::Default_Public;
        return 0;
    }

    int handleDescribe(const RtspRequest&req,RtspResponse& res)
    {
        std::cout<<"recv request:\n"
                 <<req.toString();
        res[RtspMessage::ContentType] = "application/sdp";
        std::string sdp = "v=0\r\no=- 0 0 IN IP4 0.0.0.0\r\nc=IN IP4 0.0.0.0\r\nt=0 0\r\na=control:*\r\n"
                          "m=video 0 RTP/AVP 96\r\na=rtpmap:96 H264/90000\r\na=control:trackID=0\r\n";   
        videoTrackId = "trackID=0";
        res.addBody(sdp);
        return 0;
    }

    int handleSetup(const RtspRequest&req,TransportV1& transport, RtspResponse& res)
    {
        std::cout<<"recv request:\n"
                <<req.toString();
        std::cout<<"tansport \n"
                << transport.toString()<<std::endl;
        if(transport.proto == UDP)
        {   
            res.setStatusCodeAndReason(RtspResponse::RTSP_Unsupported_Transport);
            return 0;
        }
        
        if(req.url().find(videoTrackId) != std::string::npos)
        {
            transport.proto = TCP;
            transport.cast = UNICAST;
            transport.tcpParam.interleaved[0] = interleaved_ * 2;
            transport.tcpParam.interleaved[1] = interleaved_ * 2 + 1;
            transports["video"] = transport;
        }
        else
        {
           res.setStatusCodeAndReason(RtspResponse::RTSP_BAD_REQUEST); 
        }
        interleaved_++;
        return 0;
    }

    int handlePlay(const RtspRequest&req,RtspResponse& res)
    {
        std::cout<<"recv request:\n"
                <<req.toString();
        url_ = req.url();
        auto url = url_;
        if(url.back() == '/')
        {
            url.pop_back();
        }
        auto pos = url.rfind('/');
        std::string filename = url.substr(pos + 1);
        startPlay = 1;
        playThread = std::thread([this,filename](){
            //wait after send play replay
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            H264MeidaSource source(GFileName);
            auto packer = H264RtpPacker(0,96,0x12345678);
            packer.setRtpCallBack([this](const uint8_t*pkg,std::size_t len) {
                sendRtpRtcp(0,pkg,len);
            });
            uint32_t ts = 0;
            
            while (!teardown)
            {
                char *frame;
                int len;
                std::tie(frame,len) = source.GetNextFrame();
                if(frame == nullptr)
                {
                    notifyClient(url_,End_of_Stream);
                    break;
                }
                if(startPlay)
                {
                    packer.pack((uint8_t*)frame,len,ts * 90);
                }
                ts += 40;
                std::this_thread::sleep_for(std::chrono::milliseconds(40));
            }
            close();
        });
        return 0;
    }

    int handlePause(const RtspRequest&req,RtspResponse& res)
    {
        startPlay = 0;
        return 0;
    }

    int handleTearDown(const RtspRequest&req,RtspResponse& res)
    {
        std::cout<<"recv request:\n"
                 <<req.toString();
        teardown = 1;
        return 0;
    }
    
    void handleRtp(int channel,const uint8_t *pkg, std::size_t len)
    {
        std::cout<<"recv chan" << channel << " length:" << len <<std::endl;
    }

    void handleTearDownResponse(const RtspResponse& res)
    {
        std::cout<<"recv teardown response \n"
                 << res.toString() <<std::endl;
    }

    void send(const std::string& msg)
    {
        {
            std::lock_guard<std::mutex> guard(mtx);
            sendList.push_back(msg);
        }
        uv_async_send(&write_async);
    }
    
protected:
    
    bool needAuth() { return true;}
    Authenticate getAuthParam() 
    { 
        Authenticate auth;
        auth.username = "test";
        auth.password = "123456";
        auth.scheme = Authenticate::AUTH_BASIC;
        auth.realm = "rtsp2-server";
        auth.nonce = Authenticate::createNonce();
        return auth;
    }

    std::string createSessionId()
    {
        std::stringstream ss;
        ss << std::hex << this;
        return ss.str();
    }

private:
    int interleaved_ = 0;
    int startPlay = 0;
    int teardown = 0;
    std::string videoTrackId = "";
    std::unordered_map<std::string,TransportV1> transports; 
    uv_tcp_t *sess;
    uv_write_t wh;
    uv_async_t write_async;
    uv_async_t close_async;
    std::thread playThread;
    std::mutex mtx;
    std::deque<std::string> sendList;
    bool writeInProgress;
    char buf[65535];
    uv_buf_t uvbuf;
    std::once_flag flag;
    std::string url_;
};

void on_new_connection(uv_stream_t *server, int status)
{
    if (status < 0)
    {
        std::cout<<"New connection error " << uv_strerror(status) << std::endl;
        return;
    }
    uv_tcp_t *client = new uv_tcp_t();
    uv_tcp_init(uv_default_loop(), client);
    if (uv_accept(server, (uv_stream_t *)client) == 0)
    {
        auto sess = new Session(client);
        sess->start();
    }
}

int main(int argc, char *argv[])
{
    GFileName = argv[1];
    uv_tcp_t server_;
    uv_tcp_init(uv_default_loop(), &server_);
    uv_tcp_nodelay(&server_, 1);
    sockaddr_in addr;
    uv_ip4_addr("0.0.0.0",15555,&addr);
    uv_tcp_bind(&server_, (const struct sockaddr *)&addr, 0);
    uv_listen((uv_stream_t *)&server_, 32, on_new_connection);
    uv_run(uv_default_loop(),UV_RUN_DEFAULT);
}

