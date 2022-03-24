#include <sstream>

#include "Rtsp2HeaderField.h"
#include "Rtsp2Utility.h"

namespace rtsp2
{

    void RtspRtpInfo::parse(const std::string& rtpinfo)
    {
        auto infos = splitString(rtpinfo,{','});
        for(auto && s : infos)
        {
            RtpInfoElement element;
            auto eles = splitString(s,';');
            if(eles.empty())
            {
                continue;
            }
            for(auto && e : eles)
            {
                std::string key = "";
                std::string value = "";
                splitInTwoPart(e,'=',key,value);
                if(key == "url")
                {
                    element.url = value;
                }
                else if(key == "seq")
                {
                    element.seq = std::stoi(value);
                }
                else if(key == "rtptime")
                {
                    element.rtptime = std::stol(value);
                }
            }
            rtpInfos.push_back(std::move(element));
        }
    }

    std::string RtspRtpInfo::toString()
    {
        std::string rtpinfo = "";
        for(auto && e : rtpInfos)
        {
            if(e.url == "")
            {
                continue;
            }
            rtpinfo += "url=" + e.url + ";seq=" + std::to_string(e.seq);
            if(e.rtptime >= 0)
            {
                rtpinfo += "rtptime=" + std::to_string(e.rtptime);
            }
            rtpinfo += ',';
        }
        rtpinfo.pop_back();
        return rtpinfo;
    }

    void RtspSessionId::parse(const std::string& session)
    {
        auto pos = session.find("timeout=");
        if(pos != std::string::npos)
        {
            sessionid = session.substr(0,pos);
            sessionid = trimSpace(session);
            timeout = std::stoi(session.substr(pos + 8));
        }
        else 
        {
            sessionid = trimSpace(session);
            timeout = 60;
        }
    }

    std::string RtspSessionId::toString()
    {
        std::string sessionHeadFiled = sessionid;
        
        if(timeout > 0 && timeout != 60)
        {
            sessionHeadFiled += ";timeout=" + std::to_string(timeout);
        }
        return sessionHeadFiled;
    }

    void Range::parse(const std::string& range)
    {
        auto ranges = splitString(range,{';'});
        for(auto && r : ranges) 
        {
            std::string key = "",value = "";
            std::string start = "",end = "";
            splitInTwoPart(r,'=',key,value);
            splitInTwoPart(value,'-',start,end);
            if(key == "time")
            {
                utcTime = parseUTCClock(value);
            }
            else if(key == "npt")
            {   
                type = NPT;
                if(start == "now")
                    rangetime.npt.isNow = 1;
                else
                    rangetime.npt.begin = parseNPTTime(start);
                    
                if(end != "")
                {
                    rangetime.npt.hasEnd = 1;
                    rangetime.npt.end = parseNPTTime(end);
                }
            }
            else if(key == "clock")
            {
                type = UTC;
                rangetime.clock.begin = parseUTCClock(start);
                if(end != "")
                {
                    rangetime.clock.hasEnd = 1;
                    rangetime.clock.end = parseUTCClock(end);
                }
            }
        }
    }

    std::string Range::toString()
    {
        std::stringstream ss;
        switch (type)
        {
        case NPT:
            ss << "npt=" << (rangetime.npt.isNow ? "now" : NPTTimeToString(rangetime.npt.begin))
               << "-" << (rangetime.npt.hasEnd ? NPTTimeToString(rangetime.npt.end) : "");
            break;
        case UTC:
            ss << "clock=" << UTCClockToString(rangetime.clock.begin)
               << "-" << (rangetime.npt.hasEnd ? UTCClockToString(rangetime.clock.end) : "");
            break;
        default:
            break;
        }
        if(utcTime > 0)
        {
            ss << ";" << UTCClockToString(utcTime);
        }
        return ss.str();
    }   

}









