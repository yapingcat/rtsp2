#ifndef RTSP2_Client_H
#define RTSP2_Client_H

#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>

#include "Rtsp2Request.h"
#include "Rtsp2Response.h"
#include "Rtsp2Transport.h"
#include "Rtsp2Authenticate.h"
#include "Rtsp2Url.h"
#include "Rtsp2Sdp.h"
#include "Rtsp2HeaderField.h"

namespace rtsp2
{
    template <typename T,typename C>
    class ClientHandle
    {
    public:
        using OutputCallback = std::function<void(const std::string &)>;
        using OptionsCB = std::function<void(T &, const RtspResponse &)>;
        using DescribeCB = std::function<void(T &, const RtspResponse &, const Sdp &sdp)>;
        using SetupCB = std::function<void(T &,  const RtspResponse &,const RtspSessionId& sessionid, std::string& media, const C&)>;
        using PlayCB = std::function<void(T &, const RtspResponse &)>;
        using AnnounceCB = std::function<void(T &, const RtspResponse &)>;
        using PauseCB = std::function<void(T &, const RtspResponse &)>;
        using SetParametersCB = std::function<void(T &, const RtspResponse &)>;
        using GetParametersCB = std::function<void(T &, const RtspResponse &)>;
        using TearDownCB = std::function<void(T &, const RtspResponse &)>;
        using REDIRECTCB = std::function<void(T &, const RtspResponse &)>;
        using OnRtp = std::function<void(T &,const std::string &media, int isRtcp, const uint8_t *, std::size_t len)>;
        using AuthFailedCB = std::function<void(T &,const std::string& username, const std::string& password)>;

    public:
        void setOutputCB(OutputCallback cb) { out_ = std::move(cb); }
        void setOptionsCB(OptionsCB cb) { optionsCb_ = std::move(cb); }
        void setDescribeCB(DescribeCB cb) { describeCb_ = std::move(cb); }
        void setSetupCB(SetupCB cb) { setupCb_ = std::move(cb); }
        void setPlayCB(PlayCB cb) { playCb_ = std::move(cb); }
        void setPauseCB(PauseCB cb) { pauseCb_ = std::move(cb); }
        void setSetParameterCB(SetParametersCB cb) { setpCb_ = std::move(cb); }
        void setGetParameterCB(GetParametersCB cb) { getpCb_ = std::move(cb); }
        void setTearDownCB(TearDownCB cb) { teardownCb_ = std::move(cb); }
        void setRtpCB(OnRtp cb) { rtpCb_ = std::move(cb); }
        void setAnnounceCB(AnnounceCB cb) { announceCb_ = std::move(cb);}
        void setRedirectCB(REDIRECTCB cb) { redirectCb_ = std::move(cb);}
        void setAuthFailedCB(AuthFailedCB cb) { authCb_ = std::move(cb);}

    protected:
        OutputCallback out_;
        OptionsCB optionsCb_;
        DescribeCB describeCb_;
        SetupCB setupCb_;
        PlayCB playCb_;
        PauseCB pauseCb_;
        SetParametersCB setpCb_;
        GetParametersCB getpCb_;
        TearDownCB teardownCb_;
        AnnounceCB announceCb_;
        REDIRECTCB redirectCb_;
        OnRtp rtpCb_;
        AuthFailedCB authCb_;
    };

    class Client : public ClientHandle<Client,TransportV1>
    {
    public:
        using RecordCB = std::function<void(Client &, const RtspResponse &)>;
        using TarnsportSet = std::vector<std::tuple<std::string,TransportV1>>;
    public:
        Client(const std::string &url);
        Client(const std::string &url, const std::string username, const std::string pwd);
        ~Client() = default;

    public:
        int options();
        int describe();
        int setup(std::vector<std::tuple<std::string,TransportV1>> transports);
        int setup(const std::string& media, const TransportV1& transports);
        int play();
        int play(const std::map<std::string,std::string>& headFiled);
        int play(const std::string& media);
        int play(const std::string& media,const std::map<std::string,std::string>& headFiled);
        int announce(const std::string &sdp);
        int record();
        int record(const std::string& media);
        int pause();
        int pause(const std::string& media);
        int setParmeters(const string &param);
        int getParmeters();
        int teardown();
        int teardown(const std::string& media);

        int pushInterleavedBinaryData(uint8_t channel,const uint8_t *pkg, std::size_t len);
        //int sendRequest(RtspRequest& req);

    public:
        
        const Url& url() const{ return url_; }
        
        void setUserInfo(const std::string username, const std::string pwd);
        
        int input(const char *data, std::size_t length);

        void setRecordCB(RecordCB cb) { recordCb_ = std::move(cb);}

    private:
        void handleUnAuth(const RtspResponse &res);
        int handleResponse(const RtspResponse &res);
        int handleDescribeResponse(const RtspResponse &res);
        int handleSetupResponse(const RtspResponse &res);
        int handlePlayResponse(const RtspResponse &res);
        int handleRecordResponse(const RtspResponse &res);
        int handlePauseResponse(const RtspResponse &res);
        int handleTeardownResponse(const RtspResponse &res);
        void processSdp(const std::string& sdp, std::string& baseUrl);
        
    private:
        RecordCB recordCb_;
        Url url_;
        bool needAuth_ = false;
        Authenticate auth_;
        Sdp sdp_;
        int cseq_ = 0;
        RtspSessionId sessionId_;
        TarnsportSet transports_;
        TarnsportSet::iterator setupIter_;
        std::vector<uint8_t> cache_;
        bool needMoreRtspMessage_ = false;
        bool isAggregate_ = false;
        uint32_t mediaStep_ = 0;
    };
} // namespace rtsp2

#endif
