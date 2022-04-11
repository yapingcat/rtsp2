#ifndef RTSP2_MESSAGE_H
#define RTSP2_MESSAGE_H

#include <string>
#include <map>
#include <cstdint>

struct http_parser;
namespace rtsp2
{
    int onHeaderField(http_parser *parser, const char *at, size_t Length);
    int onHeaderValue(http_parser *parser, const char *at, size_t Length);
    int onBody(http_parser *parser, const char *at, size_t Length);
    int onHeaderCompleted(http_parser *parser);
    int onMessageCompleted(http_parser *parser);
    class RtspMessage
    {
        friend int onHeaderField(http_parser *parser, const char *at, size_t Length);
        friend int onHeaderValue(http_parser *parser, const char *at, size_t Length);
        friend int onBody(http_parser *parser, const char *at, size_t Length);
        friend int onHeaderCompleted(http_parser *parser);
        friend int onMessageCompleted(http_parser *parser);

    public:
        enum VERSION
        {
            RTSP_1_0 = 0,
            RTSP_2_0 = 1,
        };

        struct HeadField
        {
            std::string field;
            std::string value;
        };

    public:
        RtspMessage() = default;
        virtual ~RtspMessage() = default;
        explicit RtspMessage(VERSION ver);
        RtspMessage(const RtspMessage &) = default;
        RtspMessage &operator=(const RtspMessage &) = default;
        RtspMessage(RtspMessage &&) = default;
        RtspMessage &operator=(RtspMessage &&) = default;

    public:
        VERSION version();
        void setVersion(VERSION ver);
        bool hasField(const std::string &key);
        bool hasField(const std::string &key) const;
        std::string &operator[](const std::string &key);
        const std::string &operator[](const std::string &key) const;
        void addField(const std::map<std::string,std::string> &headFileds);
        void addBody(const std::string &body);
        void addBody(const char *body, std::size_t size);
        const std::string& body() const;
        virtual int read(const char *buf, std::size_t size) = 0;
        virtual std::string toString() const;

    public:
        const static std::string ContentBase;
        const static std::string ContentLocation;
        const static std::string ContentLength;
        const static std::string ContentType;
        const static std::string CSeq;
        const static std::string Date;
        const static std::string Public;
        const static std::string Session;
        const static std::string Transport;
        const static std::string Authenticate;
        const static std::string Authorization;
        const static std::string UserAgent;
        const static std::string Server;
        const static std::string RTPInfo;
        const static std::string Range;
        const static std::string PipelinedRequests;
        const static std::string NotifyReason;
        const static std::string Default_USERAGENT;
        const static std::string Default_SERVER;
        const static std::string Default_Public;

    protected:
        VERSION ver_ = RTSP_1_0;
        std::string body_ = "";
        mutable std::map<std::string, std::string> headFileds_;
        int completed_ = 0;
        std::string fileds_ = "";
    };    
} // namespace rtsp2

#endif
