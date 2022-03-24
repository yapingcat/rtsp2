#ifndef RTSP2_RESPONSE_H
#define RTSP2_RESPONSE_H

#include "Rtsp2Message.h"

namespace rtsp2
{
    int onStatus(http_parser *parser, const char *at, size_t Length);
    class RtspResponse : public RtspMessage
    {
        friend int onStatus(http_parser *parser, const char *at, size_t Length);
    public:
        enum STATUS_CODE
        {
            RTSP_OK = 200,
            RTSP_MOVED_Permanently = 300,
            RTSP_MOVED_Temporarily = 301,
            RTSP_BAD_REQUEST = 400,
            RTSP_Unauthorized = 401,
            RTSP_Not_Found = 404,
            RTSP_Session_Not_Found = 454,
            RTSP_Unsupported_Transport = 461,
            RTSP_Internal_Server_Error = 500,
            RTSP_Not_Implemented = 501,
            RTSP_Version_Not_Supported = 505,
        };

    public:
        RtspResponse() = default;
        RtspResponse(VERSION ver);
        RtspResponse(VERSION ver, STATUS_CODE code);
        ~RtspResponse() = default;

    public:
        STATUS_CODE statusCode() const;
        void setStatusCode(STATUS_CODE code);
        const std::string &reason() const;
        void setReason(const std::string &reason);
        void setStatusCodeAndReason(STATUS_CODE code);

        int read(const char *buf, std::size_t len) override;

        std::string toString() const override;

    public:
        static std::string covertCodeToReason(STATUS_CODE code);

    private:
        STATUS_CODE code_ = RTSP_OK;
        std::string reason_ = "OK";
    };

    RtspResponse makeResponse(RtspMessage::VERSION ver, RtspResponse::STATUS_CODE code);

} // namespace rtsp2

#endif
