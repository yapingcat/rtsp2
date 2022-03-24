#ifndef RTSP2_SERVER_H
#define RTSP2_SERVER_H

#include <vector>
#include <unordered_map>
#include "Rtsp2Request.h"
#include "Rtsp2Response.h"
#include "Rtsp2Transport.h"
#include "Rtsp2Authenticate.h"

namespace rtsp2
{
    class ServerHandle
    {
    public:
        ServerHandle() = default;
        virtual ~ServerHandle() = default;

    public:
        int input(const char* msg,int len);
        
    protected:

        // method            direction        object     requirement
        // DESCRIBE          C->S             P,S        recommended
        // ANNOUNCE          C->S, S->C       P,S        optional
        // GET_PARAMETER     C->S, S->C       P,S        optional
        // OPTIONS           C->S, S->C       P,S        required
        //                                                 (S->C: optional)
        // PAUSE             C->S             P,S        recommended
        // PLAY              C->S             P,S        required
        // RECORD            C->S             P,S        optional
        // REDIRECT          S->C             P,S        optional
        // SETUP             C->S             S          required
        // SET_PARAMETER     C->S, S->C       P,S        optional
        // TEARDOWN          C->S             P,S        required

        virtual void handleOption(const RtspRequest&req,RtspResponse& res){}
        virtual void handleDescribe(const RtspRequest&req,RtspResponse& res){}
        virtual void handleSetup(const RtspRequest&req,TransportV1& transport, RtspResponse& res){}
        virtual void handlePlay(const RtspRequest&req,RtspResponse& res){}
        virtual void handlePause(const RtspRequest&req,RtspResponse& res){}
        virtual void handleTearDown(const RtspRequest&req,RtspResponse& res){}
        virtual void handleAnnounce(const RtspRequest&req,RtspResponse& res){}
        virtual void handleRecord(const RtspRequest&req,RtspResponse& res){}
        virtual void handleSetParmeters(const RtspRequest&req,RtspResponse& res){}
        virtual void handleGetParmeters(const RtspRequest&req,RtspResponse& res){}
        virtual void handleRtp(int channel,const uint8_t *pkg, std::size_t len){};
        virtual void handleOptionResponse(const RtspResponse& res){}
        virtual void handleAnnounceResponse(const RtspResponse& res){}
        virtual void handleGetParameterResponse(const RtspResponse& res){}
        virtual void handleSetParameterResponse(const RtspResponse& res){}

        virtual void send(const std::string& msg) = 0;
        int sendRtspMessage(RtspRequest req);
        int sendRtpRtcp(int channel,const uint8_t *pkg, std::size_t len);
        
    protected:
        virtual bool needAuth() { return false;}
        virtual Authenticate getAuthParam() { return Authenticate();}
        virtual std::string createSessionId() = 0;
        bool verifyAuthParams(const RtspRequest& req);
        const std::string& sessionId() { return sessionId_;}

    private:
        int handleResponse(const uint8_t* res,int len);
        int handleRequest(const uint8_t* req,int len);
        void replaySetup(const RtspRequest&req,RtspResponse& res);

    private:
        std::vector<uint8_t> cache_;
        Authenticate authParam_;
        bool needMoreRtspMessage_ = false;
        RtspRequest currentReq_;
        std::string sessionId_ = "";
    };
}



#endif