#include "Rtsp2Request.h"
#include "Rtsp2Utility.h"
#include "http-parser/http_parser.h"
#include <sstream>
#include <iostream>

namespace rtsp2
{
    int onurl(http_parser *parser, const char *at, size_t Length)
    {
        auto req = (RtspRequest*)parser->data;
        req->url_.assign(at,Length);
        return 0;
    }

    const std::string RtspRequest::OPTIONS = "OPTIONS";
    const std::string RtspRequest::DESCRIBE = "DESCRIBE";
    const std::string RtspRequest::SETUP = "SETUP";
    const std::string RtspRequest::PLAY = "PLAY";
    const std::string RtspRequest::TEARDOWN = "TEARDOWN";
    const std::string RtspRequest::ANNOUNCE = "ANNOUNCE";
    const std::string RtspRequest::PAUSE = "PAUSE";
    const std::string RtspRequest::GET_PARAMETER = "GET_PARAMETER";
    const std::string RtspRequest::SET_PARAMETER = "SET_PARAMETER";
    const std::string RtspRequest::REDIRECT = "REDIRECT";
    const std::string RtspRequest::RECORD = "RECORD";
    const std::string RtspRequest::PLAYNOTIFY = "PLAY_NOTIFY";

    RtspRequest::RtspRequest(VERSION ver)
        : RtspRequest(ver, "", "")
    {
    }

    RtspRequest::RtspRequest(VERSION ver, const std::string &method)
        : RtspRequest(ver, method, "")
    {
    }

    RtspRequest::RtspRequest(VERSION ver, const std::string &method, const std::string &url)
        : RtspMessage(ver),url_(url), method_(method)
    {
    }

    const std::string &RtspRequest::method() const
    {
        return method_;
    }

    void RtspRequest::setMethod(const std::string &method)
    {
        method_ = method;
    }

    const std::string &RtspRequest::url() const
    {
        return url_;
    }

    void RtspRequest::setUrl(const std::string &url)
    {
        url_ = url;
    }

    int RtspRequest::read(const char *buf, std::size_t size)
    {
        http_parser_settings setting;
        http_parser parser;
        parser.data = this;
        http_parser_init(&parser, HTTP_REQUEST);
        http_parser_settings_init(&setting);
        setting.on_url = onurl;
        setting.on_header_field = onHeaderField;
        setting.on_header_value = onHeaderValue;
        setting.on_body = onBody;
        setting.on_headers_complete = onHeaderCompleted;
        setting.on_message_complete = onMessageCompleted;

        int ret = http_parser_execute(&parser, &setting, buf, size);
        if (parser.http_errno != HPE_OK)
        {
            return -1;
        }
        std::cout<<ret<<std::endl;
        if (completed_)
        {
            method_ = http_method_str((http_method)parser.method);
            std::cout<<method_<<std::endl;
            return ret;
        }
        else
        {
            return 0;
        }
    }

    std::string RtspRequest::toString() const
    {
        std::stringstream ss;

        ss << method_ << " "
           << url_ << " "
           << (ver_ == RTSP_1_0 ? "RTSP/1.0" : "RTSP/2.0") << "\r\n";

        if (!body_.empty())
        {
            headFileds_[ContentLength] = std::to_string(body_.size());
        }
        for (auto &&hf : headFileds_)
        {
            ss << hf.first << ": " << hf.second << "\r\n";
        }
        ss << "\r\n";
        ss << body_;
        return ss.str();
    }

   

    RtspRequest makeRequest(RtspRequest::VERSION ver, const std::string &method, const std::string &url)
    {
        RtspRequest req(ver, method, url);
        req[RtspMessage::UserAgent] = RtspMessage::Default_USERAGENT;
        req[RtspMessage::Date] = getGMTTime();
        return req;
    }

    RtspRequest makeOptions(const std::string &url, RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::OPTIONS, url);
    }

    RtspRequest makeDescribe(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::DESCRIBE, url);
    }

    RtspRequest makeSetup(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::SETUP, url);
    }

    RtspRequest makePlay(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::PLAY, url);
    }

    RtspRequest makeTeardown(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::TEARDOWN, url);
    }

    RtspRequest makePause(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::PAUSE, url);
    }

    RtspRequest makeAnnounce(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::ANNOUNCE, url);
    }

    RtspRequest makeRecord(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::RECORD, url);
    }

    RtspRequest makeSetParameter(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::SET_PARAMETER, url);
    }

    RtspRequest makeGetParameter(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::GET_PARAMETER, url);
    }

    RtspRequest makePlayNotify(const std::string &url,RtspMessage::VERSION ver)
    {
        return makeRequest(ver, RtspRequest::PLAYNOTIFY, url);
    }
}
