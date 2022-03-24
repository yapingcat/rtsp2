#include "Rtsp2Server.h"
#include "Rtsp2Utility.h"

#include <iostream>
#include <sstream>
#include <cassert>

namespace rtsp2
{
    int ServerHandle::input(const char* msg,int length)
    {
        const uint8_t*p = (const uint8_t*)msg;
        int len = length;
        if(!cache_.empty())
        {
            cache_.insert(cache_.end(),msg,msg+len);
            p = cache_.data();
            len = cache_.size();
        }

        while(len > 0 )
        {  
            int ret = 0;
            if(*p == '$' && !needMoreRtspMessage_)
            {
                uint8_t channel = 0;
                uint16_t packetLength = 0;
                const uint8_t *pkg;
                std::tie(ret, channel, packetLength, pkg) = rtpOverRtsp(p, len);
                if(ret < 0)
                {
                    return ret;
                }
                else if(ret == 0)
                {
                    break;
                }
                handleRtp(channel,pkg,packetLength);
            }
            else
            {
                if((ret = handleRequest(p,len)) < 0 && (ret = handleResponse(p,len)) < 0)
                {
                    std::cout<<"parser rtsp msg failed"<<std::endl;
                    return ret;
                }
                else if(ret == 0)
                {
                    needMoreRtspMessage_ = true;
                    break;
                }
                else
                {
                    needMoreRtspMessage_ = false;
                }
            }
            if(ret > 0)
            {
                p += ret;
                len -= ret;
            }
        }
        if (cache_.empty())
        {
            if (len > 0)
            {
                cache_.insert(cache_.end(), (const uint8_t *)msg + length - len, (const uint8_t *)msg + length);
            }
        }
        else
        {
            cache_.erase(cache_.begin(), cache_.begin() + cache_.size() - len);
        }
        return 0;
    }

    int ServerHandle::sendRtspMessage(RtspRequest req)
    {
        currentReq_ = std::move(req);
        send(req.toString());
        return 0;
    }

    int ServerHandle::sendRtpRtcp(int channel,const uint8_t *pkg, std::size_t len)
    {
        std::string rtcprtp;
        rtcprtp.resize(len+4);
        rtcprtp[0] = '$';
        rtcprtp[1] = channel;
        rtcprtp[2] = (uint8_t)(len >> 8); 
        rtcprtp[3] = (uint8_t)(len);
        rtcprtp.replace(4,len,(const char*)pkg,len);
        send(rtcprtp);
        return 0;
    }

    bool ServerHandle::verifyAuthParams(const RtspRequest& req)
    {
        if(!req.hasField(RtspMessage::Authorization))
        {
            return false;
        }
        Authenticate auth;
        auth.parser(req[RtspMessage::Authorization]);
        if(authParam_.scheme != auth.scheme)
        {
            return false;
        }
        if(authParam_.scheme == Authenticate::AUTH_BASIC)
        {
            return base64Encode(authParam_.username + ":" + authParam_.password) == auth.response ? true : false;
        }
        authParam_.method = req.method();
        authParam_.uri = auth.uri;
        if(auth.nonce == authParam_.nonce && auth.response == authParam_.makeResponse())
        {
            return true;
        } 
        else 
        {
            return false;
        }
    }

    int ServerHandle::handleResponse(const uint8_t* res,int len)
    {   
        RtspResponse response;
        auto ret = response.read((const char *)res, len);
        std::cout<<"ret"<<ret<<std::endl;
        if (ret > 0)
        {
            if(response.statusCode() == RtspResponse::RTSP_Unauthorized)
            {
                if(!needAuth())
                {
                    return ret;
                }    
                authParam_ = getAuthParam();
                currentReq_[RtspMessage::Authenticate] = authParam_.credentials();
                send(currentReq_.toString());
                return ret;
            }
            if(currentReq_.method() == RtspRequest::OPTIONS) handleOptionResponse(response);
            else if(currentReq_.method() == RtspRequest::ANNOUNCE) handleAnnounceResponse(response);
            else if(currentReq_.method() == RtspRequest::GET_PARAMETER) handleGetParameterResponse(response);
            else if(currentReq_.method() == RtspRequest::SET_PARAMETER) handleSetParameterResponse(response);
        }
        return ret;
    }

    int ServerHandle::handleRequest(const uint8_t* req,int len)
    {
        RtspRequest request;
        auto ret = request.read((const char *)req, len);
        if (ret <= 0)
        {
            return ret;
        }
        RtspResponse response(RtspMessage::VERSION::RTSP_1_0);
        response.setStatusCodeAndReason(RtspResponse::RTSP_OK);
        response[RtspMessage::CSeq] = request[RtspMessage::CSeq];
        response[RtspMessage::Server] = RtspMessage::Default_SERVER;
        response[RtspMessage::Date] = getGMTTime();
        if(needAuth() && !verifyAuthParams(request))
        {
            response.setStatusCodeAndReason(RtspResponse::RTSP_Unauthorized);
            authParam_ = getAuthParam();
            response[RtspMessage::Authenticate] = authParam_.genAuthInfo();
            send(response.toString());
            return ret;
        }
        if(request.hasField(RtspMessage::Session))
        {
            if(sessionId_ != request[RtspMessage::Session])
            {
                response.setStatusCodeAndReason(RtspResponse::RTSP_Session_Not_Found);
                send(response.toString());
                return ret;
            }
        }
        if(request.method() == RtspRequest::OPTIONS) handleOption(request,response);
        else if(request.method() == RtspRequest::DESCRIBE) handleDescribe(request,response);
        else if(request.method() == RtspRequest::SETUP) replaySetup(request,response);
        else if(request.method() == RtspRequest::PLAY) handlePlay(request,response);
        else if(request.method() == RtspRequest::PAUSE) handlePause(request,response);
        else if(request.method() == RtspRequest::TEARDOWN) handleTearDown(request,response);
        else if(request.method() == RtspRequest::GET_PARAMETER) handleGetParmeters(request,response);
        else if(request.method() == RtspRequest::SET_PARAMETER) handleSetParmeters(request,response);
        else if(request.method() == RtspRequest::RECORD) handleRecord(request,response);
        else if(request.method() == RtspRequest::ANNOUNCE)  handleAnnounce(request,response);
        if(!sessionId_.empty())
        {
            response[RtspMessage::Session] = sessionId_;
        }
        std::cout<<"res:\n"
                    << response.toString()<<std::endl;
        
        send(response.toString());
        return ret;
    }


    void ServerHandle::replaySetup(const RtspRequest&req,RtspResponse& res)
    {
        if(!req.hasField(RtspMessage::Transport))
        {
            res.setStatusCodeAndReason(RtspResponse::RTSP_Unsupported_Transport);
            return;
        }

        TransportV1 transport = decodeTransportV1(req[RtspMessage::Transport]);
        handleSetup(req,transport,res);
        if(res.statusCode() == RtspResponse::RTSP_OK)
        {
            res[RtspMessage::Transport] = transport.toString();
            if(sessionId_.empty())
            {
                sessionId_ = createSessionId();
            }
        }
    }
} // namespace rtsp2
