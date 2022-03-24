#ifndef RTSP2_REQUEST_H
#define RTSP2_REQUEST_H

#include "Rtsp2Message.h"

namespace rtsp2
{
    int onurl(http_parser *parser, const char *at, size_t Length);
    class RtspRequest : public RtspMessage
    {
        friend int onurl(http_parser *parser, const char *at, size_t Length);
    public:
        RtspRequest() = default;
        RtspRequest(VERSION ver);
        RtspRequest(VERSION ver, const std::string &method, const std::string &url);
        RtspRequest(VERSION ver, const std::string &method);
        RtspRequest(const RtspRequest &) = default;
        RtspRequest &operator=(const RtspRequest &) = default;
        RtspRequest(RtspRequest &&) = default;
        RtspRequest &operator=(RtspRequest &&) = default;
        ~RtspRequest() = default;

    public:
        const std::string &method() const;
        void setMethod(const std::string &method);
        const std::string &url() const;
        void setUrl(const std::string &url);

        int read(const char *buf, std::size_t size) override;

        std::string toString() const override;

    public:
        const static std::string OPTIONS;
        const static std::string DESCRIBE;
        const static std::string SETUP;
        const static std::string PLAY;
        const static std::string TEARDOWN;
        const static std::string ANNOUNCE;
        const static std::string PAUSE;
        const static std::string GET_PARAMETER;
        const static std::string SET_PARAMETER;
        const static std::string REDIRECT;
        const static std::string RECORD;

    private:
        std::string url_;
        std::string method_;
    };

    RtspRequest makeOptions(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makeDescribe(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makeSetup(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makePlay(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makeTeardown(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makePause(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makeAnnounce(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makeRecord(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makeSetParameter(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);
    RtspRequest makeGetParameter(const std::string &url,RtspMessage::VERSION ver = RtspMessage::RTSP_1_0);

} // namespace rtsp2

#endif