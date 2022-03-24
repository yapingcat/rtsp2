#include <sstream>

#include "third/http_parser.h"
#include "Rtsp2Response.h"
#include "Rtsp2Utility.h"

namespace rtsp2
{
    int onStatus(http_parser *parser, const char *at, size_t Length)
    {
        auto res = (RtspResponse *)(parser->data);
        res->reason_.assign(at,Length);
        return 0;
    }

    RtspResponse::RtspResponse(VERSION ver)
        :RtspMessage(ver)
    {

    }

    RtspResponse::RtspResponse(VERSION ver, STATUS_CODE code)
        :RtspResponse(ver)
    {
        code_ = code;
    }

    RtspResponse::STATUS_CODE RtspResponse::statusCode() const
    {
        return code_;
    }

    void RtspResponse::setStatusCode(STATUS_CODE code)
    {
        code_ = code;
    }

    const std::string &RtspResponse::reason() const
    {
        return reason_;
    }

    void RtspResponse::setReason(const std::string &reason)
    {
        reason_ = reason;
    }

    void RtspResponse::setStatusCodeAndReason(STATUS_CODE code)
    {
        code_ = code;
        reason_ = covertCodeToReason(code);
    }

    std::string RtspResponse::covertCodeToReason(STATUS_CODE code)
    {
        switch (code)
        {
        case STATUS_CODE::RTSP_OK:
            return "OK";
        case STATUS_CODE::RTSP_MOVED_Permanently:
            return "Moved Permanently";
        case STATUS_CODE::RTSP_MOVED_Temporarily:
            return "Moved Temporarily";
        case STATUS_CODE::RTSP_BAD_REQUEST:
            return "Bad Request";
        case STATUS_CODE::RTSP_Unauthorized:
            return "Unauthorized";
        case STATUS_CODE::RTSP_Not_Found:
            return "Not Found";
        case STATUS_CODE::RTSP_Session_Not_Found:
            return "Session Not Found";
        case STATUS_CODE::RTSP_Unsupported_Transport:
            return "Unsupported transport";
        case STATUS_CODE::RTSP_Internal_Server_Error:
            return "Internal Server Error";
        case STATUS_CODE::RTSP_Not_Implemented:
            return "Not Implemented";
        case STATUS_CODE::RTSP_Version_Not_Supported:
            return "RTSP Version not supported";
        default:
            break;
        }
        return "UnKonw Reason";
    }

    int RtspResponse::read(const char *buf, std::size_t len)
    {
        http_parser_settings setting;
        http_parser parser;
        parser.data = this;
        http_parser_init(&parser, HTTP_RESPONSE);
        http_parser_settings_init(&setting);
        setting.on_status = onStatus;
        setting.on_header_field = onHeaderField;
        setting.on_header_value = onHeaderValue;
        setting.on_body = onBody;
        setting.on_headers_complete = onHeaderCompleted;
        setting.on_message_complete = onMessageCompleted;

        int ret = http_parser_execute(&parser, &setting, buf, len);
        if (parser.http_errno != HPE_OK)
        {
            return -1;
        }

        if (completed_)
        {
            code_ = (STATUS_CODE)parser.status_code;
            return ret;
        }
        else
        {
            return 0;
        }
    }

    std::string RtspResponse::toString() const
    {
        std::stringstream ss;
        if(!body_.empty())
        {
            headFileds_[RtspMessage::ContentLength] = std::to_string(body_.size());
        }
        ss << (ver_ == RTSP_1_0 ? "RTSP/1.0" : "RTSP/2.0") << " " << code_ <<" " << reason_<<"\r\n";
        for (auto &&hf : headFileds_)
        {
            ss << hf.first << ": " << hf.second << "\r\n";
        }
        ss << "\r\n";
        ss << body_;
        return ss.str();
    }

    RtspResponse makeResponse(RtspMessage::VERSION ver, RtspResponse::STATUS_CODE code)
    {
        RtspResponse res(ver, code);
        res[RtspMessage::Server] = RtspMessage::Default_SERVER;
        res[RtspMessage::Date] = getGMTTime();
        return res;
    }

}