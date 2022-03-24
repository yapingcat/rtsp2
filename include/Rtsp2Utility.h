#ifndef RTSP2_UTILITY_H
#define RTSP2_UTILITY_H

#include <vector>
#include <string>
#include <functional>
#include <ctime>

#include "Rtsp2Response.h"

namespace rtsp2
{

    void splitString(const std::string &str, char c, const std::function<void(const char *, size_t)> &func);
    void splitString(const std::string &str, const std::vector<char> &vc, std::function<void(const char *, size_t)> func);
    int splitInTwoPart(const std::string &str, const char c, std::string &left, std::string &right);

    bool startWithString(const std::string &str, const std::string &startStr);

    std::vector<std::string> splitString(const std::string &str, char c);
    std::vector<std::string> splitString(const std::string &str, const std::vector<char> &vc);

    std::string trimSpace(const std::string &s);

    //this end with \r \n \r\n
    void readLines(const std::string &str, const std::function<void(const char *, size_t)> &func);

    std::string getGMTTime();


    bool isAbslouteUrl(const std::string& url);
    std::string lookupBaseUrl(const RtspResponse &res,const std::string& requestUrl);

    std::string base64Encode(const std::string &plainTxt);

    std::string base64Decode(const std::string &base64String);

        
    using Interleaved_Rtp = std::tuple<int, uint8_t, uint16_t, const uint8_t *>;

    Interleaved_Rtp rtpOverRtsp(const uint8_t *pkg, std::size_t len);

    uint64_t parseUTCClock(const std::string& clock);
    std::string UTCClockToString(uint64_t utc);
    uint64_t parseNPTTime(const std::string& npt);
    std::string NPTTimeToString(uint64_t npt);

    uint16_t randomInt(int max);
    
    class MD5
    {
    public:
        void sum(const std::string &content);
        void sum(const char *data, std::size_t len);
        void sumFile(const std::string &path);
        std::string final();
        void reset();

    private:
        void processChunk(const uint8_t *chunk);

    private:
        uint32_t a_ = 0x67452301;
        uint32_t b_ = 0xEFCDAB89;
        uint32_t c_ = 0x98BADCFE;
        uint32_t d_ = 0x10325476;
        std::string cache_ = "";
        uint64_t total_ = 0;
    };


}

#endif
