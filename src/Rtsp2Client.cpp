#include <algorithm>
#include <cassert>
#include <iostream>

#include "Rtsp2Client.h"
#include "Rtsp2Utility.h"
#include "Rtsp2Err.h"

namespace rtsp2
{
    #define MakeV1RequstByName(Name,req) req = make##Name(auth_.uri); \
                                         if(needAuth_) { \
                                            req[RtspMessage::Authorization] = auth_.credentials(); \
                                         } \
                                         req[RtspMessage::CSeq] = std::to_string(cseq_++); \
                                         if(!sessionId_.sessionid.empty()) \
                                         {\
                                            req[RtspMessage::Session] = sessionId_.sessionid; \
                                         }\
                                         if(pipelinedRequestId_ > 0) \
                                         {\
                                            req[RtspMessage::PipelinedRequests] = std::to_string(pipelinedRequestId_);\
                                            pipelines_[pipelinedRequestId_].push_back(req);\
                                         }\
    
    Client::Client(const std::string &url)
    {
        if(parserUrl(url,url_) != 0)
        {
            throw std::invalid_argument("url is invaild");
        }
        auth_.username = url_.username;
        auth_.password = url_.passwd;
        if(auth_.username.empty() && auth_.password.empty())
        {
            auth_.uri = url;
        }
        else
        {
            auto pos = url.find("@");
            auth_.uri = url_.scheme + "://" + url.substr(pos+1);
        }
    }

    Client::Client(const std::string &url, const std::string username, const std::string pwd)
        :Client(url)
    {
        auth_.username = username;
        auth_.password = pwd;
    }

    void Client::setUserInfo(const std::string username, const std::string pwd)
    {
        auth_.username = username;
        auth_.password = pwd;
    }

    std::error_code Client::input(const char *data, std::size_t length)
    {
        const uint8_t *p = (const uint8_t *)data;
        int len = length;
        if (!cache_.empty())
        {
            cache_.insert(cache_.end(), (const uint8_t *)data, (const uint8_t *)data + length);
            p = cache_.data();
            len = cache_.size();
        }
        while (len > 0)
        {
            int ret = 0;
            if (*p == '$' && !needMoreRtspMessage_)
            {
                uint8_t channel = 0;
                uint16_t packetLength = 0;
                const uint8_t *pkg;
                std::tie(ret, channel, packetLength, pkg) = rtpOverRtsp(p, len);
                if (ret == 0)
                {
                    break;
                }
                else if (ret < 0)
                {
                    return makeError(RTSP2_ERROR::rtsp2_rtp_parser_failed);
                }
                for (auto &&mediaTrans : transports_)
                {
                    auto && transport = std::get<1>(mediaTrans);
                    if (transport.tcpParam.interleaved[0] == channel || transport.tcpParam.interleaved[1] == channel)
                    {
                        if (rtpCb_)
                        {
                            rtpCb_(*this, std::get<0>(mediaTrans),transport.tcpParam.interleaved[0] == channel ? 0 : 1, pkg, packetLength);
                        }
                        break;
                    }
                }
                p += ret;
                len -= ret;
            }
            else
            {
                RtspResponse res;
                ret = res.read((const char *)p, len);
                if(ret > 0)
                {
                    needMoreRtspMessage_ = false;
                    auto ec = handleResponse(res);
                    if(ec)
                    {
                        return ec;
                    }
                    p += ret;
                    len -= ret;
                    break;
                }
                else if (ret == 0)
                {
                    needMoreRtspMessage_ = true;
                    break;
                }

                RtspRequest req;
                ret = req.read((const char *)p, len);
                if(ret > 0)
                {
                    needMoreRtspMessage_ = false;
                    RtspResponse res(RtspMessage::VERSION::RTSP_1_0);
                    res.setStatusCodeAndReason(RtspResponse::RTSP_OK);
                    res[RtspMessage::CSeq] = req[RtspMessage::CSeq];
                    res[RtspMessage::Server] = RtspMessage::Default_SERVER;
                    res[RtspMessage::Date] = getGMTTime();
                    if(requestCb_)
                    {
                        requestCb_(req,res);
                    }
                    else
                    {
                        res.setStatusCodeAndReason(RtspResponse::RTSP_Not_Implemented);
                    }
                    out_(res.toString());
                    p += ret;
                    len -= ret;
                    break;
                }
                else if (ret == 0)
                {
                    needMoreRtspMessage_ = true;
                    break;
                }
                return makeError(RTSP2_ERROR::rtsp2_msg_parser_failed);
            }
        }

        if (cache_.empty())
        {
            if (len > 0)
            {
                cache_.insert(cache_.end(), (const uint8_t *)data + length - len, (const uint8_t *)data + length);
            }
        }
        else
        {
            cache_.erase(cache_.begin(), cache_.begin() + cache_.size() - len);
        }
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }
    
    std::error_code Client::options()
    {
        auth_.method = RtspRequest::OPTIONS;
        RtspRequest req;
        MakeV1RequstByName(Options,req);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::describe()
    {
        auth_.method = RtspRequest::DESCRIBE;
        RtspRequest req;
        MakeV1RequstByName(Describe,req);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::setup(std::vector<std::tuple<std::string,TransportV1>> transports)
    {
        if(transports.size() == 0)
        {
            return makeError(RTSP2_ERROR::rtsp2_need_transport);
        }

        transports_ = std::move(transports);
        setupIter_ = transports_.begin();
        std::error_code ec;
        do  {
            setup(std::get<0>(*setupIter_),std::get<1>(*setupIter_));
        }while(!ec && pipelinedRequestId_ > 0 && (++setupIter_) != transports_.end());
        return ec;
    }

    std::error_code Client::setup(const std::string& media, const TransportV1& transports)
    {
        auth_.method = RtspRequest::SETUP;
        for(auto && md : sdp_.mediaDescriptions)
        {
            if(md.media.media == media)
            {
                auth_.uri = md.aggregateUrl;
            }
        }
        
        if(transports_.empty())
        {
            transports_.emplace_back(media,transports);
            setupIter_ = transports_.end() - 1;
        }
        
        RtspRequest req;
        MakeV1RequstByName(Setup,req);
        req[RtspMessage::Transport] = transports.toString();
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::play()
    {
        return play(std::map<std::string,std::string>{});
    }

    std::error_code Client::play(const std::map<std::string,std::string>& headFiled)
    {
        auth_.method = RtspRequest::PLAY;
        if(isAggregate_)
        {
            auth_.uri = sdp_.sessionDescription.aggregateUrl;
            RtspRequest req;
            MakeV1RequstByName(Play,req);
            req.addField(headFiled);
            out_(req.toString());
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        else
        {
            std::error_code ec;
            do {
                ec = play(sdp_.mediaDescriptions[mediaStep_++].media.media,headFiled);
            }while(!ec && pipelinedRequestId_ > 0 && mediaStep_ < sdp_.mediaDescriptions.size());
            return ec;
        }
    }

    std::error_code Client::play(const std::string& media)
    {
       return play(media,std::map<std::string,std::string>{});
    }

    std::error_code Client::play(const std::string& media,const std::map<std::string,std::string>& headFiled)
    {
        auth_.method = RtspRequest::PLAY;
        auto found = std::find_if(sdp_.mediaDescriptions.begin(),sdp_.mediaDescriptions.end(),[&media](const MediaDescription& md) {
            if(media == md.media.media)
            {
                return true;
            }
            else 
            {
                return false;
            }
        });

        if(found == sdp_.mediaDescriptions.end())
        {
            return makeError(RTSP2_ERROR::rtsp2_media_not_exist);
        }
        auth_.uri = found->aggregateUrl;
        RtspRequest req;
        MakeV1RequstByName(Play,req);
        req.addField(headFiled);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::announce(const std::string &sdp)
    {
        processSdp(sdp,auth_.uri);
        auth_.method = RtspRequest::ANNOUNCE;
        RtspRequest req;
        MakeV1RequstByName(Announce,req);
        req[RtspMessage::ContentType] = "application/sdp";
        req.addBody(sdp);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::record()
    {
        auth_.method = RtspRequest::RECORD;
        if(isAggregate_)
        {
            auth_.uri = sdp_.sessionDescription.aggregateUrl;
            RtspRequest req;
            MakeV1RequstByName(Record,req);
            out_(req.toString());
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        else
        {
            std::error_code ec;
            do {
                ec = record(sdp_.mediaDescriptions[mediaStep_++].media.media);
            }while(!ec && pipelinedRequestId_ > 0 && mediaStep_ < sdp_.mediaDescriptions.size());
            return ec;
        }
    }
    
    std::error_code Client::record(const std::string& media)
    {
        auth_.method = RtspRequest::RECORD;
        auto found = std::find_if(sdp_.mediaDescriptions.begin(),sdp_.mediaDescriptions.end(),[&media](const MediaDescription& md) {
            if(media == md.media.media)
            {
                return true;
            }
            else 
            {
                return false;
            }
        });

        if(found == sdp_.mediaDescriptions.end())
        {
            return makeError(RTSP2_ERROR::rtsp2_media_not_exist);;
        }
        auth_.uri = found->aggregateUrl;
        RtspRequest req;
        MakeV1RequstByName(Record,req);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }
    

    std::error_code Client::pause()
    {
        auth_.method = RtspRequest::PAUSE;
        if(isAggregate_)
        {
            auth_.uri = sdp_.sessionDescription.aggregateUrl;
            RtspRequest req;
            MakeV1RequstByName(Pause,req);
            out_(req.toString());
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        else
        {
            std::error_code ec;
            do {
                ec = pause(sdp_.mediaDescriptions[mediaStep_++].media.media);
            }while(!ec && pipelinedRequestId_ > 0 && mediaStep_ < sdp_.mediaDescriptions.size());
            return ec;
        }
    }

    std::error_code Client::pause(const std::string& media)
    {
        auth_.method = RtspRequest::PAUSE;
        auto found = std::find_if(sdp_.mediaDescriptions.begin(),sdp_.mediaDescriptions.end(),[&media](const MediaDescription& md) {
            if(media == md.media.media)
            {
                return true;
            }
            else 
            {
                return false;
            }
        });

        if(found == sdp_.mediaDescriptions.end())
        {
            return makeError(RTSP2_ERROR::rtsp2_media_not_exist);
        }
        auth_.uri = found->aggregateUrl;
        RtspRequest req;
        MakeV1RequstByName(Pause,req);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::setParmeters(const string &param)
    {
        auth_.method = RtspRequest::SET_PARAMETER;
        RtspRequest req;
        MakeV1RequstByName(SetParameter,req);
        req.addBody(param);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::getParmeters()
    {
        auth_.method = RtspRequest::GET_PARAMETER;
        RtspRequest req;
        MakeV1RequstByName(GetParameter,req);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::teardown()
    {
        auth_.method = RtspRequest::TEARDOWN;
        if(isAggregate_)
        {
            auth_.uri = sdp_.sessionDescription.aggregateUrl;
            RtspRequest req;
            MakeV1RequstByName(Teardown,req);
            out_(req.toString());
        }
        else
        {
            return teardown(sdp_.mediaDescriptions[mediaStep_++].media.media);
        }
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::teardown(const std::string& media)
    {
        auth_.method = RtspRequest::TEARDOWN;
        auto found = std::find_if(sdp_.mediaDescriptions.begin(),sdp_.mediaDescriptions.end(),[&media](const MediaDescription& md) {
            if(media == md.media.media)
            {
                return true;
            }
            else 
            {
                return false;
            }
        });

        if(found == sdp_.mediaDescriptions.end())
        {
            return makeError(RTSP2_ERROR::rtsp2_media_not_exist);
        }
        auth_.uri = found->aggregateUrl;
        RtspRequest req;
        MakeV1RequstByName(Teardown,req);
        out_(req.toString());
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::pushInterleavedBinaryData(uint8_t channel,const uint8_t *pkg, std::size_t len)
    {
        std::string msg(len + 4,0);
        msg[0] = '$';
        msg[1] = channel;
        msg[2] = uint8_t(len >> 8);
        msg[3] = uint8_t(len);
        msg.replace(4,len,(const char*)pkg,len);
        out_(msg);
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }
    
    void Client::startPipeline()
    {
        pipelinedRequestId_ = randomInt(65535);
    }

    void Client::stopPipeline()
    {
        pipelinedRequestId_ = 0;
    }

    // int Client::sendRequest(RtspRequest& req)
    // {
    //     req[RtspMessage::CSeq] = std::to_string(cseq_);
    //     cseq_++;
    //     if(!sessionId_.sessionid.empty())
    //     {
    //         req[RtspMessage::Session] = sessionId_.sessionid;
    //     }
    //     out_(req.toString());
    // }
    

    void Client::handleUnAuth(const RtspResponse &res)
    {
        auto authInfo = res[RtspMessage::Authenticate];
        auth_.parser(authInfo);
        needAuth_ = true;
        RtspRequest req(RtspMessage::RTSP_1_0, auth_.method, auth_.uri);
        req[RtspMessage::UserAgent] = RtspMessage::Default_USERAGENT;
        req[RtspMessage::Date] = getGMTTime();
        req[RtspMessage::Authorization] = auth_.credentials();
        req[RtspMessage::CSeq] = cseq_++;
        out_(req.toString());
        return;
    }


    std::error_code Client::handleResponse(const RtspResponse &res)
    {
        if (res.statusCode() == RtspResponse::RTSP_Unauthorized)
        {
            if(needAuth_)
            {
                authCb_(*this,auth_.username,auth_.password);
            }
            else 
            {
                handleUnAuth(res);
            }
           return makeError(RTSP2_ERROR::rtsp2_ok);
        }

        if(!sessionId_.sessionid.empty() && !res.hasField(RtspMessage::Session))
        {
             return makeError(RTSP2_ERROR::rtsp2_session_id_error);
        }
        
        if(res.hasField(RtspMessage::PipelinedRequests))
        {
            return handlePipelineResponse(res);
        }
        
        if(auth_.method == RtspRequest::OPTIONS) optionsCb_(*this,res);
        else if(auth_.method == RtspRequest::DESCRIBE) return handleDescribeResponse(res);   
        else if(auth_.method == RtspRequest::SETUP) return handleSetupResponse(res);
        else if(auth_.method == RtspRequest::PLAY) return handlePlayResponse(res);
        else if(auth_.method == RtspRequest::TEARDOWN) return handleTeardownResponse(res);
        else if(auth_.method == RtspRequest::PAUSE)  return handlePauseResponse(res);
        else if(auth_.method == RtspRequest::SET_PARAMETER) setpCb_(*this,res);
        else if(auth_.method == RtspRequest::GET_PARAMETER) getpCb_(*this,res);
        else if(auth_.method == RtspRequest::RECORD) handleRecordResponse(res);
        else if(auth_.method == RtspRequest::ANNOUNCE) announceCb_(*this,res);
        else if(auth_.method == RtspRequest::REDIRECT) redirectCb_(*this,res);
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }


    std::error_code Client::handleDescribeResponse(const RtspResponse &res)
    {
        auto baseurl = lookupBaseUrl(res,auth_.uri);
        processSdp(res.body(),baseurl);
        describeCb_(*this,res,sdp_);
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::handleSetupResponse(const RtspResponse &res)
    {
        std::string mediaName;
        TransportV1 transport;
        std::tie(mediaName,transport) = *setupIter_;
        if(res.statusCode() != RtspResponse::RTSP_OK)
        {
            setupCb_(*this,res,sessionId_, mediaName, transport);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }

        TransportV1 trans = decodeTransportV1(res[RtspMessage::Transport]);
        if(sessionId_.sessionid.empty() && res.hasField(RtspMessage::Session))
        {   
            sessionId_.parse(res[RtspMessage::Session]);
        }
        
        std::get<1>(*setupIter_) = trans;
        setupCb_(*this,res, sessionId_,mediaName, trans);
        setupIter_++;
        if(setupIter_ != transports_.end())
        {
            setup(std::get<0>(*setupIter_),std::get<1>(*setupIter_));
        }
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    std::error_code Client::handlePlayResponse(const RtspResponse& res)
    {
        if(isAggregate_)
        {
            playCb_(*this,res);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }

        if(res.statusCode() != RtspResponse::RTSP_OK || mediaStep_ >= sdp_.mediaDescriptions.size())
        {
            playCb_(*this,res);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        else
        {
            return play(sdp_.mediaDescriptions[mediaStep_++].media.media);
        }
    }

    std::error_code Client::handleRecordResponse(const RtspResponse &res)
    {
        if(isAggregate_)
        {
            recordCb_(*this,res);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        if(res.statusCode() != RtspResponse::RTSP_OK || mediaStep_ >= sdp_.mediaDescriptions.size())
        {
            recordCb_(*this,res);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        else
        {
            return record(sdp_.mediaDescriptions[mediaStep_++].media.media);
        }
    }

    std::error_code Client::handlePauseResponse(const RtspResponse &res)
    {
        if(isAggregate_)
        {
            pauseCb_(*this,res);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        if(mediaStep_ >= sdp_.mediaDescriptions.size())
        {
            pauseCb_(*this,res);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        else
        {
            return pause(sdp_.mediaDescriptions[mediaStep_++].media.media);
        }
    }

    std::error_code Client::handleTeardownResponse(const RtspResponse &res)
    {
        if(isAggregate_)
        {
            teardownCb_(*this,res);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        if(mediaStep_ >= sdp_.mediaDescriptions.size())
        {
            teardownCb_(*this,res);
            return makeError(RTSP2_ERROR::rtsp2_ok);
        }
        else
        {
            return teardown(sdp_.mediaDescriptions[mediaStep_++].media.media);
        }
    }

    std::error_code Client::handlePipelineResponse(const RtspResponse &res)
    {
        auto pipeid = std::stoi(res[RtspMessage::PipelinedRequests]);
        auto found = std::find_if(pipelines_[pipeid].begin(),pipelines_[pipeid].end(),[&res](const RtspRequest& req){
            return req[RtspMessage::CSeq] == res[RtspMessage::CSeq];
        });

        if(found == pipelines_[pipeid].end())
        {
            return makeError(RTSP2_ERROR::rtsp2_pipeline_not_found);
        }
        
        if(found->method() == RtspRequest::OPTIONS) optionsCb_(*this,res);
        else if(found->method() == RtspRequest::DESCRIBE) return handleDescribeResponse(res);   
        else if(found->method() == RtspRequest::PLAY) playCb_(*this,res);
        else if(found->method() == RtspRequest::TEARDOWN) teardownCb_(*this,res);
        else if(found->method() == RtspRequest::PAUSE)  pauseCb_(*this,res);
        else if(found->method() == RtspRequest::SET_PARAMETER) setpCb_(*this,res);
        else if(found->method() == RtspRequest::GET_PARAMETER) getpCb_(*this,res);
        else if(found->method() == RtspRequest::RECORD) recordCb_(*this,res);
        else if(found->method() == RtspRequest::ANNOUNCE) announceCb_(*this,res);
        else if(found->method() == RtspRequest::REDIRECT) redirectCb_(*this,res);
        else if(found->method() == RtspRequest::SETUP) 
        {
            TransportV1 trans = decodeTransportV1(res[RtspMessage::Transport]);
            if(sessionId_.sessionid.empty() && res.hasField(RtspMessage::Session))
            {   
                sessionId_.parse(res[RtspMessage::Session]);
            }
            for(auto && md : sdp_.mediaDescriptions)
            {
                if(md.aggregateUrl != found->url())
                {
                    continue;
                }
                setupCb_(*this,res,sessionId_,md.media.media,trans);
            }
        }
        
        pipelines_[pipeid].erase(found);
        return makeError(RTSP2_ERROR::rtsp2_ok);
    }

    void Client::processSdp(const std::string& sdp,std::string& baseUrl)
    {
        sdp_ = parser(sdp);
        if(sdp_.sessionDescription.aggregateUrl != "")
        {
            isAggregate_ = true;
            if(sdp_.sessionDescription.aggregateUrl == "*")
            {
                sdp_.sessionDescription.aggregateUrl = baseUrl;
            }
            else if(!isAbslouteUrl(sdp_.sessionDescription.aggregateUrl))
            {
                if(baseUrl.back() == '/')
                {
                    baseUrl.pop_back();
                }
                sdp_.sessionDescription.aggregateUrl = baseUrl + "/" + sdp_.sessionDescription.aggregateUrl;
            }
        }
        for(auto && md : sdp_.mediaDescriptions)
        {
            if(md.aggregateUrl.empty())
            {
                assert(sdp_.mediaDescriptions.size() == 1); 
                if(isAggregate_)
                {
                    md.aggregateUrl = sdp_.sessionDescription.aggregateUrl;
                }
                else
                {
                    md.aggregateUrl = auth_.uri;
                }
            }
            else
            {
                if(!isAbslouteUrl(md.aggregateUrl))
                {
                    if(md.aggregateUrl.front() == '/')
                    {
                        md.aggregateUrl = md.aggregateUrl.substr(1);
                    }
                    if(isAggregate_)
                    {
                        if(sdp_.sessionDescription.aggregateUrl.back() != '/')
                        {
                             md.aggregateUrl = sdp_.sessionDescription.aggregateUrl + "/" + md.aggregateUrl;
                        }
                        else 
                        {
                            md.aggregateUrl = sdp_.sessionDescription.aggregateUrl + md.aggregateUrl;
                        }
                    }
                    else
                    {
                        if(baseUrl.back() == '/')
                        {
                            baseUrl.pop_back();
                        }
                        md.aggregateUrl = baseUrl + "/" + md.aggregateUrl;
                    }
                }  
            }
        }
    }
}
