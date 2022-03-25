#ifndef RTSP2_SERVER_H
#define RTSP2_SERVER_H

#include <vector>
#include <deque>
#include <unordered_map>
#include <system_error>

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
        std::error_code input(const char* msg,int len);
        
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

        //handleXXXX接口
        /// @return 0 -success 可以发送response 1 - 当前无法处理,不能发送response,后续通过
        virtual int handleOption(const RtspRequest&req,RtspResponse& res){ return 0;}
        virtual int handleDescribe(const RtspRequest&req,RtspResponse& res){return 0;}
        virtual int handleSetup(const RtspRequest&req,TransportV1& transport, RtspResponse& res){return 0;}
        virtual int handlePlay(const RtspRequest&req,RtspResponse& res){return 0;}
        virtual int handlePause(const RtspRequest&req,RtspResponse& res){return 0;}
        virtual int handleTearDown(const RtspRequest&req,RtspResponse& res){return 0;}
        virtual int handleAnnounce(const RtspRequest&req,RtspResponse& res){return 0;}
        virtual int handleRecord(const RtspRequest&req,RtspResponse& res){return 0;}
        virtual int handleSetParmeters(const RtspRequest&req,RtspResponse& res){return 0;}
        virtual int handleGetParmeters(const RtspRequest&req,RtspResponse& res){return 0;}
        virtual void handleRtp(int channel,const uint8_t *pkg, std::size_t len){};
        virtual void handleOptionResponse(const RtspResponse& res){}
        virtual void handleAnnounceResponse(const RtspResponse& res){}
        virtual void handleGetParameterResponse(const RtspResponse& res){}
        virtual void handleSetParameterResponse(const RtspResponse& res){}
        virtual void handleTearDownResponse(const RtspResponse& res){}

        virtual void send(const std::string& msg) = 0;
        
        std::error_code sendRtspMessage(RtspRequest req);
        std::error_code sendResponse(RtspResponse res);
        std::error_code sendRtpRtcp(int channel,const uint8_t *pkg, std::size_t len);
        
        
    protected:
        virtual bool needAuth() { return false;}
        virtual Authenticate getAuthParam() { return Authenticate();}
        virtual std::string createSessionId() = 0;
        bool verifyAuthParams(const RtspRequest& req);

    private:
        int handleResponse(const uint8_t* res,int len);
        int handleRequest(const uint8_t* req,int len);
        int replaySetup(const RtspRequest&req,RtspResponse& res);

    private:
        std::deque<RtspRequest> pipelineReqests_;
        std::vector<uint8_t> cache_;
        Authenticate authParam_;
        bool needMoreRtspMessage_ = false;
        RtspRequest currentReq_;
        std::string sessionId_ = "";
        std::vector<RtspRequest> prePipeRequest_;
    };
}



#endif
