#include <iostream>
#include "Rtsp2Message.h"
#include "http-parser/http_parser.h"

namespace rtsp2
{

    int onHeaderField(http_parser *parser, const char *at, size_t Length)
    {
        auto msg = (RtspMessage *)(parser->data);
        msg->fileds_.assign(at, Length);
        return 0;
    }

    int onHeaderValue(http_parser *parser, const char *at, size_t Length)
    {
        auto msg = (RtspMessage *)(parser->data);
        msg->headFileds_[msg->fileds_] = {at, Length};
        return 0;
    }

    int onBody(http_parser *parser, const char *at, size_t Length)
    {
        auto msg = (RtspMessage *)(parser->data);
        msg->body_.assign(at, Length);
        return 0;
    }

    int onHeaderCompleted(http_parser *parser)
    {
        auto msg = (RtspMessage *)(parser->data);
        if(!msg->hasField(RtspMessage::ContentLength))
        {
            return 2;
        }
        return 0;
    }

    int onMessageCompleted(http_parser *parser)
    {
        std::cout<<"on message completesd"<<std::endl;
        auto msg = (RtspMessage *)(parser->data);
        msg->completed_ = 1;
        return 0;
    }

    const std::string RtspMessage::ContentBase = "Content-Base";
    const std::string RtspMessage::ContentLocation = "Content-Location";
    const std::string RtspMessage::ContentLength = "Content-Length";
    const std::string RtspMessage::ContentType = "Content-Type";
    const std::string RtspMessage::CSeq = "CSeq";
    const std::string RtspMessage::Date = "Date";
    const std::string RtspMessage::Public = "Public";
    const std::string RtspMessage::Session = "Session";
    const std::string RtspMessage::Transport = "Transport";
    const std::string RtspMessage::Authenticate = "WWW-Authenticate";
    const std::string RtspMessage::Authorization = "Authorization";
    const std::string RtspMessage::UserAgent = "User-Agent";
    const std::string RtspMessage::Server = "Server";
    const std::string RtspMessage::RTPInfo = "RTP-Info";
    const std::string RtspMessage::Range = "Range";
    const std::string RtspMessage::PipelinedRequests = "Pipelined-Requests";
    const std::string RtspMessage::NotifyReason = "Notify-Reason";
    const std::string RtspMessage::Default_USERAGENT = "rtsp2-client";
    const std::string RtspMessage::Default_SERVER = "rtsp2-server";
    const std::string RtspMessage::Default_Public = "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, ANNOUNCE, RECORD, SET_PARAMETER, GET_PARAMETER";

    RtspMessage::RtspMessage(VERSION ver)
        : ver_(ver)
    {
    }

    RtspMessage::VERSION RtspMessage::version()
    {
        return ver_;
    }

    void RtspMessage::setVersion(VERSION ver)
    {
        ver_ = ver;
    }

    bool RtspMessage::hasField(const std::string &key)
    {
        return headFileds_.count(key) > 0 ? true : false;
    }

     bool RtspMessage::hasField(const std::string &key) const
    {
        return headFileds_.count(key) > 0 ? true : false;
    }

    std::string &RtspMessage::operator[](const std::string &key)
    {
        return headFileds_[key];
    }

    const std::string &RtspMessage::operator[](const std::string &key) const
    {
        if (!hasField(key))
        {
            throw std::invalid_argument(key + " not exist");
        }
        return headFileds_[key];
    }

    void RtspMessage::addField(const std::map<std::string,std::string> &headFileds)
    {
        for(auto&& hf : headFileds)
        {
            headFileds_.insert(hf);
        }
    }

    void RtspMessage::addBody(const std::string &body)
    {
        body_.append(body);
    }

    void RtspMessage::addBody(const char *body, std::size_t size)
    {
        body_.append(body, size);
    }

    const std::string& RtspMessage::body() const
    {
        return body_;
    }

    std::string RtspMessage::toString() const
    {
        return "";
    }

}