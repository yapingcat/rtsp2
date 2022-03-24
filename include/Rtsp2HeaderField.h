#ifndef RTSP2_HEAD_FILED_H
#define RTSP2_HEAD_FILED_H

#include <cstdint>
#include <string>
#include <vector>

namespace rtsp2
{
    struct RtspRtpInfo
    {
        struct  RtpInfoElement 
        {
            std::string url;
            uint16_t seq;
            uint64_t rtptime;
        };
        std::vector<RtpInfoElement> rtpInfos;
        void parse(const std::string& rtpinfo);
        std::string toString();
    };

    struct RtspSessionId
    {
        std::string sessionid;
        uint32_t timeout  = 60;
        
        void parse(const std::string& session);
        std::string toString();
    };

    enum RangeType
    { 
        NPT = 0,
        UTC = 1,
        //SMPTE
    };

    union RangeTime
    {
        struct NPT
        {
            int isNow = 0;
            uint64_t begin = 0; //单位毫秒
            int hasEnd = 0;
            uint64_t end = 0; //单位毫秒
        } npt;

        struct UTC
        {
            uint64_t begin = 0; //单位毫秒
            int hasEnd = 0;
            uint64_t end = 0; //单位毫秒
        }clock;
    };
    

    struct Range 
    {
        RangeType type;
        RangeTime rangetime;
        uint64_t utcTime = 0;   
        void parse(const std::string& range);
        std::string toString();     
    };

}



#endif
