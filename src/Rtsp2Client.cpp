#include <algorithm>
#include <cassert>
#include <iostream>

#include "Rtsp2Client.h"
#include "Rtsp2Utility.h"

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

    int Client::input(const char *data, std::size_t length)
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
            std::cout<<__LINE__<<len<<std::endl;
            int ret = 0;
            if (*p == '$' && !needMoreRtspMessage_)
            {
                uint8_t channel = 0;
                uint16_t packetLength = 0;
                const uint8_t *pkg;
                std::tie(ret, channel, packetLength, pkg) = rtpOverRtsp(p, len);
                if (ret == 0)
                {
                    std::cout<<"rtp need more bytes "<<packetLength <<std::endl;
                    break;
                }
                else if (ret < 0)
                {
                    std::cout<<"parser rtp failed" <<std::endl; 
                    return ret;
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
                std::cout<<ret<<std::endl;
                p += ret;
                len -= ret;
            }
            else
            {
                RtspResponse res;
                ret = res.read((const char *)p, len);
                std::cout<<__LINE__ <<"ret"<<ret<<std::endl;
                if (ret < 0)
                {
                    std::cout<<"parser rtsp message failed" <<std::endl; 
                    return ret;
                }
                else if (ret == 0)
                {
                    needMoreRtspMessage_ = true;
                    break;
                }
                else
                {
                    needMoreRtspMessage_ = false;
                    if(handleResponse(res) != 0)
                    {
                        return -1;
                    }
                    std::cout<<__LINE__<<ret<<std::endl;
                    p += ret;
                    len -= ret;
                }
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
            std::cout<<__LINE__<<len<<std::endl;
            cache_.erase(cache_.begin(), cache_.begin() + cache_.size() - len);
        }
        return 0;
    }
    
    int Client::options()
    {
        auth_.method = RtspRequest::OPTIONS;
        RtspRequest req;
        MakeV1RequstByName(Options,req);
        out_(req.toString());
        return 0;
    }

    int Client::describe()
    {
        auth_.method = RtspRequest::DESCRIBE;
        RtspRequest req;
        MakeV1RequstByName(Describe,req);
        out_(req.toString());
        return 0;
    }

    int Client::setup(std::vector<std::tuple<std::string,TransportV1>> transports)
    {
        if(transports.size() == 0)
        {
            return -1;
        }

        transports_ = std::move(transports);
        setupIter_ = transports_.begin();
        setup(std::get<0>(*setupIter_),std::get<1>(*setupIter_));
        return 0;
    }

    int Client::setup(const std::string& media, const TransportV1& transports)
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
        return 0;
    }

    int Client::play()
    {
        return play(std::map<std::string,std::string>{});
    }

    int Client::play(const std::map<std::string,std::string>& headFiled)
    {
        auth_.method = RtspRequest::PLAY;
        if(isAggregate_)
        {
            auth_.uri = sdp_.sessionDescription.aggregateUrl;
            RtspRequest req;
            MakeV1RequstByName(Play,req);
            req.addField(headFiled);
            out_(req.toString());
            return 0;
        }
        else
        {
            return play(sdp_.mediaDescriptions[mediaStep_++].media.media,headFiled);
        }
    }

    int Client::play(const std::string& media)
    {
       return play(media,std::map<std::string,std::string>{});
    }

    int Client::play(const std::string& media,const std::map<std::string,std::string>& headFiled)
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
            return -1;
        }
        auth_.uri = found->aggregateUrl;
        RtspRequest req;
        MakeV1RequstByName(Play,req);
        req.addField(headFiled);
        out_(req.toString());
        return 0;
    }

    int Client::announce(const std::string &sdp)
    {
        processSdp(sdp,auth_.uri);
        auth_.method = RtspRequest::ANNOUNCE;
        RtspRequest req;
        MakeV1RequstByName(Announce,req);
        req[RtspMessage::ContentType] = "application/sdp";
        req.addBody(sdp);
        out_(req.toString());
        return 0;
    }

    int Client::record()
    {
        auth_.method = RtspRequest::RECORD;
        if(isAggregate_)
        {
            auth_.uri = sdp_.sessionDescription.aggregateUrl;
            RtspRequest req;
            MakeV1RequstByName(Record,req);
            out_(req.toString());
        }
        else
        {
            return record(sdp_.mediaDescriptions[mediaStep_++].media.media);
        }
        return 0;
    }
    
    int Client::record(const std::string& media)
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
            return -1;
        }
        auth_.uri = found->aggregateUrl;
        RtspRequest req;
        MakeV1RequstByName(Record,req);
        out_(req.toString());
        return 0;
    }
    

    int Client::pause()
    {
        auth_.method = RtspRequest::PAUSE;
        if(isAggregate_)
        {
            auth_.uri = sdp_.sessionDescription.aggregateUrl;
            RtspRequest req;
            MakeV1RequstByName(Pause,req);
            out_(req.toString());
        }
        else
        {
            return pause(sdp_.mediaDescriptions[mediaStep_++].media.media);
        }
        return 0;
    }

    int Client::pause(const std::string& media)
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
            return -1;
        }
        auth_.uri = found->aggregateUrl;
        RtspRequest req;
        MakeV1RequstByName(Pause,req);
        out_(req.toString());
        return 0;
    }

    int Client::setParmeters(const string &param)
    {
        auth_.method = RtspRequest::SET_PARAMETER;
        RtspRequest req;
        MakeV1RequstByName(SetParameter,req);
        req.addBody(param);
        out_(req.toString());
        return 0;
    }

    int Client::getParmeters()
    {
        auth_.method = RtspRequest::GET_PARAMETER;
        RtspRequest req;
        MakeV1RequstByName(GetParameter,req);
        out_(req.toString());
        return 0;
    }

    int Client::teardown()
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
        return 0;
    }

    int Client::teardown(const std::string& media)
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
            return -1;
        }
        auth_.uri = found->aggregateUrl;
        RtspRequest req;
        MakeV1RequstByName(Teardown,req);
        out_(req.toString());
        return 0;
    }

    int Client::pushInterleavedBinaryData(uint8_t channel,const uint8_t *pkg, std::size_t len)
    {
        std::string msg(len + 4,0);
        msg[0] = '$';
        msg[1] = channel;
        msg[2] = uint8_t(len >> 8);
        msg[3] = uint8_t(len);
        msg.replace(4,len,(const char*)pkg,len);
        out_(msg);
        return 0;
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

    int Client::handleResponse(const RtspResponse &res)
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
           return 0;
        }

        if(!sessionId_.sessionid.empty() && !res.hasField(RtspMessage::Session))
        {
            return -1;
        }

        if(auth_.method == RtspRequest::OPTIONS) optionsCb_(*this,res);
        else if(auth_.method == RtspRequest::DESCRIBE) return handleDescribeResponse(res);   
        else if(auth_.method == RtspRequest::SETUP)
        {
            return handleSetupResponse(res);
        }     
        else if(auth_.method == RtspRequest::PLAY)
        {
            return handlePlayResponse(res);
        }
        else if(auth_.method == RtspRequest::TEARDOWN)
        {
            return handleTeardownResponse(res);
        }
        else if(auth_.method == RtspRequest::PAUSE)
        {
            handlePauseResponse(res);
        }
         else if(auth_.method == RtspRequest::SET_PARAMETER)
        {
            setpCb_(*this,res);
        }
         else if(auth_.method == RtspRequest::GET_PARAMETER)
        {
            getpCb_(*this,res);
        }
         else if(auth_.method == RtspRequest::RECORD)
        {
            handleRecordResponse(res);
        }
        else if(auth_.method == RtspRequest::ANNOUNCE)
        {
            announceCb_(*this,res);
        }
        else if(auth_.method == RtspRequest::REDIRECT)
        {
            redirectCb_(*this,res);
        }
        return 0;
    }


    int Client::handleDescribeResponse(const RtspResponse &res)
    {
        auto baseurl = lookupBaseUrl(res,auth_.uri);
        processSdp(res.body(),baseurl);
        describeCb_(*this,res,sdp_);
        return 0;
    }

    int Client::handleSetupResponse(const RtspResponse &res)
    {
        std::string mediaName;
        TransportV1 transport;
        std::tie(mediaName,transport) = *setupIter_;
        if(res.statusCode() != RtspResponse::RTSP_OK)
        {
            setupCb_(*this,res,sessionId_, mediaName, transport);
            return 0;
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
        return 0;
    }

    int Client::handlePlayResponse(const RtspResponse& res)
    {
        if(isAggregate_)
        {
            playCb_(*this,res);
            return 0;
        }

        if(res.statusCode() != RtspResponse::RTSP_OK || mediaStep_ >= sdp_.mediaDescriptions.size())
        {
            playCb_(*this,res);
            return 0;
        }
        else
        {
            play(sdp_.mediaDescriptions[mediaStep_++].media.media);
            return 0;
        }
    }

    int Client::handleRecordResponse(const RtspResponse &res)
    {
        if(isAggregate_)
        {
            recordCb_(*this,res);
            return 0;
        }
        if(res.statusCode() != RtspResponse::RTSP_OK || mediaStep_ >= sdp_.mediaDescriptions.size())
        {
            recordCb_(*this,res);
            return 0;
        }
        else
        {
            record(sdp_.mediaDescriptions[mediaStep_++].media.media);
            return 0;
        }
    }

    int Client::handlePauseResponse(const RtspResponse &res)
    {
        if(isAggregate_)
        {
            pauseCb_(*this,res);
            return 0;
        }
        if(mediaStep_ >= sdp_.mediaDescriptions.size())
        {
            pauseCb_(*this,res);
            return 0;
        }
        else
        {
            pause(sdp_.mediaDescriptions[mediaStep_++].media.media);
            return 0;
        }
    }

    int Client::handleTeardownResponse(const RtspResponse &res)
    {
        if(isAggregate_)
        {
            teardownCb_(*this,res);
            return 0;
        }
        if(mediaStep_ >= sdp_.mediaDescriptions.size())
        {
            teardownCb_(*this,res);
            return 0;
        }
        else
        {
            teardown(sdp_.mediaDescriptions[mediaStep_++].media.media);
            return 0;
        }
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
