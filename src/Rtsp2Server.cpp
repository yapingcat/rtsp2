#include <iostream>
#include <sstream>
#include <cassert>

#include "Rtsp2Server.h"
#include "Rtsp2Utility.h"
#include "Rtsp2Err.h"

namespace rtsp2
{
    std::error_code ServerHandle::input(const char* msg,int length)
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
                    return makeError(RTSP2_ERROR::rtsp2_rtp_parser_failed);
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
                    return makeError(RTSP2_ERROR::rtsp2_msg_parser_failed);
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
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code ServerHandle::notifyClient(const std::string& url,NotifyReasonType reasonType)
    {
        auto req = makePlayNotify(url);
        req[RtspMessage::NotifyReason] = (reasonType == End_of_Stream ? "end-of-stream" 
                                            : (reasonType == Media_Properties_Update ? "media-properties-update" 
                                                : (reasonType == Scale_Change ? "scale-change" : "")));
        return sendRtspMessage(req);
    }

    std::error_code ServerHandle::sendRtspMessage(RtspRequest req)
    {
        currentReq_ = std::move(req);
        currentReq_[RtspMessage::Session] = sessionId_;
        currentReq_[RtspMessage::CSeq] = std::to_string(randomInt(1000));
        send(currentReq_.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code ServerHandle::sendResponse(RtspResponse res)
    {
        if(!sessionId_.empty())
        {
            res[RtspMessage::Session] = sessionId_;
        }

        if(pipelineReqests_.empty())
        {
            return makeError(RTSP2_ERROR::rtsp2_pipeline_empty);
        }
        RtspRequest& req = pipelineReqests_.front();
        res[RtspMessage::CSeq] = req[RtspMessage::CSeq];
        if(req.hasField(RtspMessage::PipelinedRequests))
        {
            res[RtspMessage::PipelinedRequests] = req[RtspMessage::PipelinedRequests];
        }
        send(res.toString());
        pipelineReqests_.pop_front();
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code ServerHandle::sendRtpRtcp(int channel,const uint8_t *pkg, std::size_t len)
    {
        std::string rtcprtp;
        rtcprtp.resize(len+4);
        rtcprtp[0] = '$';
        rtcprtp[1] = channel;
        rtcprtp[2] = (uint8_t)(len >> 8); 
        rtcprtp[3] = (uint8_t)(len);
        rtcprtp.replace(4,len,(const char*)pkg,len);
        send(rtcprtp);
        return makeError(RTSP2_ERROR::rtsp2_ok);
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
        if (ret > 0)
        {
            //  服务端发送request到客户端需要鉴权吗?
            // if(response.statusCode() == RtspResponse::RTSP_Unauthorized)
            // {
            //     if(!needAuth())
            //     {
            //         return ret;
            //     }    
            //     authParam_ = getAuthParam();
            //     currentReq_[RtspMessage::Authenticate] = authParam_.credentials();
            //     send(currentReq_.toString());
            //     return ret;
            // }
            if(currentReq_.method() == RtspRequest::OPTIONS) handleOptionResponse(response);
            else if(currentReq_.method() == RtspRequest::ANNOUNCE) handleAnnounceResponse(response);
            else if(currentReq_.method() == RtspRequest::GET_PARAMETER) handleGetParameterResponse(response);
            else if(currentReq_.method() == RtspRequest::SET_PARAMETER) handleSetParameterResponse(response);
            else if(currentReq_.method() == RtspRequest::TEARDOWN) handleTearDownResponse(response);
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

        bool isPipelineRequest = request.hasField(RtspMessage::PipelinedRequests);
        if(isPipelineRequest)
        {
            if(!prePipeRequest_.empty() 
                && prePipeRequest_[0][RtspMessage::PipelinedRequests] != request[RtspMessage::PipelinedRequests])
            {
                prePipeRequest_.pop_back();
            }
        }
        else
        {
            if(!prePipeRequest_.empty())
            {
                prePipeRequest_.pop_back();
            }
        }
        
        bool prePipeReqHasSessionId = !prePipeRequest_.empty() && prePipeRequest_[0].hasField(RtspMessage::Session);
        if(request.hasField(RtspMessage::Session))
        {
            if(sessionId_ != request[RtspMessage::Session])
            {
                response.setStatusCodeAndReason(RtspResponse::RTSP_Session_Not_Found);
                send(response.toString());
                return ret;
            }
        }
        else
        {
            if(!sessionId_.empty() && (!isPipelineRequest || prePipeReqHasSessionId))
            {
                response.setStatusCodeAndReason(RtspResponse::RTSP_Session_Not_Found);
                send(response.toString());
                return ret;
            }
        }

        int r = 0;
        if(request.method() == RtspRequest::OPTIONS) r = handleOption(request,response);
        else if(request.method() == RtspRequest::DESCRIBE) r = handleDescribe(request,response);
        else if(request.method() == RtspRequest::SETUP) r = replaySetup(request,response);
        else if(request.method() == RtspRequest::PLAY) r = handlePlay(request,response);
        else if(request.method() == RtspRequest::PAUSE) r = handlePause(request,response);
        else if(request.method() == RtspRequest::TEARDOWN) r = handleTearDown(request,response);
        else if(request.method() == RtspRequest::GET_PARAMETER) r = handleGetParmeters(request,response);
        else if(request.method() == RtspRequest::SET_PARAMETER) r = handleSetParmeters(request,response);
        else if(request.method() == RtspRequest::RECORD) r = handleRecord(request,response);
        else if(request.method() == RtspRequest::ANNOUNCE)  r = handleAnnounce(request,response);

        if(r != 0)
        {
            pipelineReqests_.push_back(std::move(request));
            return ret;
        }

        if(!sessionId_.empty())
        {
            response[RtspMessage::Session] = sessionId_;
        }
        if(request.hasField(RtspMessage::PipelinedRequests))
        {
            response[RtspMessage::PipelinedRequests] = request[RtspMessage::PipelinedRequests];
        }
        std::cout<<"res:\n"
                    << response.toString()<<std::endl;
        
        send(response.toString());
        return ret;
    }


    int ServerHandle::replaySetup(const RtspRequest&req,RtspResponse& res)
    {
        if(!req.hasField(RtspMessage::Transport))
        {
            res.setStatusCodeAndReason(RtspResponse::RTSP_Unsupported_Transport);
            return 0;
        }

        TransportV1 transport = decodeTransportV1(req[RtspMessage::Transport]);
        if(handleSetup(req,transport,res) != 0 )
        {
            return -1;
        }
        if(res.statusCode() == RtspResponse::RTSP_OK)
        {
            res[RtspMessage::Transport] = transport.toString();
            if(sessionId_.empty())
            {
                sessionId_ = createSessionId();
            }
        }
        return 0;
    }
} // namespace rtsp2

