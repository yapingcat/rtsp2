#include "Rtsp2Server.h"
#include "Rtsp2Sdp.h"
#include "Rtsp2HeaderField.h"
#include <iostream>
#include <unordered_map>
#include <list>
#include "asio.hpp"

using namespace rtsp2;

class AsioRtspServerSession : public rtsp2::ServerHandle ,public std::enable_shared_from_this<AsioRtspServerSession>
{
public:
    AsioRtspServerSession(asio::ip::tcp::socket socket)
        :socket_(std::move(socket))
    {
        cache_.resize(4096);
    }

public:
    void start()
    {
        do_read();
    }

    void stop()
    {
        socket_.close();
    }

public:
    void handleOption(const RtspRequest&req,RtspResponse& res)
    {
        std::cout<<"recv request:\n"
                 <<req.toString();
        res[RtspMessage::Public] = "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, ANNOUNCE, RECORD, SET_PARAMETER, GET_PARAMETER";
        return;
    }

    void handleSetup(const RtspRequest&req,TransportV1& transport, RtspResponse& res)
    {
        std::cout<<"recv setup request\n"
                 << req.toString();
        for(auto && it : sdp_.mediaDescriptions)
        {
            std::cout<<req.url()<<std::endl;
            std::cout<<it.aggregateUrl<<std::endl;
            if(req.url().find(it.aggregateUrl) != std::string::npos)
            {
                trans[it.media.media] = transport;
                break;
            }
        }
    }

    void handleAnnounce(const RtspRequest&req,RtspResponse& res)
    {
        std::cout<<"recv Announce Requeset\n"
                 <<req.toString();
        auto sdptxt = req.body();
        sdp_ = parser(sdptxt);
    }

    void handleRecord(const RtspRequest&req,RtspResponse& res)
    {
        std::cout<<"recv Record request\n"
                 << req.toString();
        if(req.hasField(RtspMessage::RTPInfo))
        {
            RtspRtpInfo info;
            info.parse(req[RtspMessage::RTPInfo]);
            std::cout<<"rtpinfo:\n";
            for(auto&& i : info.rtpInfos)
            {
                std::cout<<"url:" << i.url << " seq:" <<i.seq <<" rtptime:" <<i.rtptime<<std::endl;
            }
        }
    }


    void handleTearDown(const RtspRequest&req,RtspResponse& res)
    {

    }

    void handleRtp(int channel,const uint8_t *pkg, std::size_t len)
    {
        std::cout<<"rtp packet channel" << channel<<std::endl;
        std::string mediaName;
        bool isRtcp = false;
        for(auto&& it : trans)
        {
            if(it.second.tcpParam.interleaved[0] == channel || it.second.tcpParam.interleaved[1] == channel)
            {
                mediaName = it.first;
                isRtcp = it.second.tcpParam.interleaved[1] == channel;
                std::cout<<"recv " << mediaName << (isRtcp ? "rtcp" : "rtp") << " packet " << "length " << len <<std::endl;
                break;
            }
        }
    }


    void send(const std::string& msg)
    {
        bool isWriting = !sendLists_.empty();
        sendLists_.push_back(msg);
        if(!isWriting)
        {
            do_write();
        }
    }


protected:

    std::string createSessionId()
    {
        std::stringstream ss;
        ss << std::hex << this;
        return ss.str();
    }

private:
    void do_write()
    {
        auto self = shared_from_this();
        asio::async_write(socket_,asio::buffer(sendLists_.front()),[this,self](const asio::error_code& ec, std::size_t len){
            if(ec)
            {
                std::cout<<"write error " << ec.message()<<std::endl;
                stop();
                return;
            }
            sendLists_.pop_front();
            if(sendLists_.empty())
            {
                return;
            }
            do_write();
        });
    }

    void do_read()
    {
        auto self = shared_from_this();
        socket_.async_read_some(asio::buffer(cache_),[this,self](asio::error_code ec,std::size_t nread){
            if(ec)
            {
                std::cout<<"read error " << ec.message()<<std::endl;
                stop();
                return;
            }
            input((const char*)cache_.data(),nread);
            do_read();
        });
    }


private:
    
    Sdp sdp_;
    std::unordered_map<std::string, TransportV1> trans;
    asio::ip::tcp::socket socket_;
    std::list<std::string> sendLists_;
    std::vector<uint8_t> cache_;
};


class Rtspserver
{
public:
  Rtspserver(asio::io_context& io_context, short port)
    : acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<AsioRtspServerSession>(std::move(socket))->start();
          }
          do_accept();
        });
  }

  asio::ip::tcp::acceptor acceptor_;
};


int main(int argc,char *argv[])
{
    asio::io_context ctx(1);
    Rtspserver server(ctx,std::atoi(argv[1]));
    ctx.run();
    return 0;
}

