#include <algorithm>
#include <cstring>
#include <ctime>
#include <array>
#include <functional>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <cstdio>

#include "Rtsp2Utility.h"


namespace rtsp2
{
    void splitString(const std::string &str, char c, const std::function<void(const char *, size_t)> &func)
    {
        auto pre = str.begin();
        for (auto &&it = str.begin(); it != str.end(); it++)
        {
            if (*it == c)
            {
                auto distanceFromBegin = std::distance(str.begin(), pre);
                auto len = std::distance(pre, it);
                if (len > 0)
                    func(str.c_str() + distanceFromBegin, len);
                pre = it + 1;
            }
        }
        if (pre != str.end())
        {
            auto distanceFromBegin = std::distance(str.begin(), pre);
            auto len = std::distance(pre, str.end());
            func(str.c_str() + distanceFromBegin, len);
        }
    }

    void splitString(const std::string &str, const std::vector<char> &vc, std::function<void(const char *, size_t)> func)
    {
        auto pre = str.begin();
        for (auto &&it = str.begin(); it != str.end(); it++)
        {
            if (std::find(vc.begin(), vc.end(), *it) != vc.end())
            {
                auto distanceFromBegin = std::distance(str.begin(), pre);
                auto len = std::distance(pre, it);
                if (len > 0)
                    func(str.c_str() + distanceFromBegin, len);
                pre = it + 1;
            }
        }
        if (pre != str.end())
        {
            auto distanceFromBegin = std::distance(str.begin(), pre);
            auto len = std::distance(pre, str.end());
            func(str.c_str() + distanceFromBegin, len);
        }
    }

    int splitInTwoPart(const std::string &str, const char c, std::string &left, std::string &right)
    {
        left = "";
        right = "";
        auto splitloc = str.find(c);
        if (splitloc == std::string::npos)
        {
            left = str;
            return 0;
        }
        left = str.substr(0, splitloc);
        right = str.substr(splitloc + 1);
        auto found = left.find_last_not_of(' ');
        if (found != std::string::npos)
            left.erase(found + 1);
        found = right.find_first_not_of(' ');
        if (found != std::string::npos)
            right = right.substr(found);
        return 0;
    }

    bool startWithString(const std::string &str, const std::string &startStr)
    {
        if (startStr.size() > str.size())
            return false;
        return std::equal(startStr.begin(), startStr.end(), str.begin());
    }

    std::vector<std::string> splitString(const std::string &str, char c)
    {
        std::vector<std::string> result;
        splitString(str, c, [&result](const char *s, size_t len)
                    { result.emplace_back(s, len); });
        return result;
    }

    std::vector<std::string> splitString(const std::string &str, const std::vector<char> &vc)
    {
        std::vector<std::string> result;
        splitString(str, vc, [&result](const char *s, size_t len)
                    { result.emplace_back(s, len); });
        return result;
    }

    std::string trimSpace(const std::string &s)
    {
        int beg = 0;
        int len = s.size();
        for (; beg < len; beg++)
        {
            if (!std::isspace(s[beg]))
            {
                break;
            }
        }

        int end = len - 1;
        for (; end >= 0; end--)
        {
            if (!std::isspace(s[end]))
            {
                break;
            }
        }
        return s.substr(beg, end - beg + 1);
    }

    void readLines(const std::string &str, const std::function<void(const char *, size_t)> &func)
    {
        std::string::size_type loc = 0;
        for (std::string::size_type i = 0; i < str.size(); i++)
        {
            char c = str[i];
            if (c == '\r' || c == '\n')
            {
                if (i - loc > 0)
                {
                    func(str.c_str() + loc, i - loc);
                }
                loc = i + 1;
            }
        }
    }

    std::string getGMTTime()
    {
        auto now = time(nullptr);
        auto now_tm = gmtime(&now);
        char str[102] = "";
        strftime(str, 102, "%d %b %C %T GMT", now_tm);
        return str;
    }

    std::string lookupBaseUrl(const RtspResponse &res,const std::string& requestUrl)
    {
        if(res.hasField(RtspMessage::ContentBase))
        {
            return res[RtspMessage::ContentBase];
        }
        if(res.hasField(RtspMessage::ContentLocation))
        {
            return res[RtspMessage::ContentLocation];
        }
        return requestUrl;
    }

    bool isAbslouteUrl(const std::string& url)
    {
        return startWithString(url,"rtsps://") || startWithString(url,"rtsp://");
    }

    static std::array<char, 64> base64IndexTable =
        {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
            'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
            'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
            'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
            'w', 'x', 'y', 'z', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '+', '/'};

    std::string base64Encode(const std::string &plainTxt)
    {
        auto length = plainTxt.size();
        auto penddingBytes = length % 3 ? 3 - length % 3 : 0;
        auto totalBytes = length / 3 * 4 + (penddingBytes ? 4 : 0);
        size_t loc = 0;
        std::string base64String(totalBytes, 0);
        for (size_t i = 0, j = 0; i < length / 3; i++)
        {
            auto v1 = base64IndexTable[plainTxt[j] >> 2];
            auto v = plainTxt[j];
            j++;
            auto v2 = base64IndexTable[(v & 0x03) << 4 | plainTxt[j] >> 4];
            v = plainTxt[j];
            j++;
            auto v3 = base64IndexTable[(v & 0x0F) << 2 | plainTxt[j] >> 6];
            auto v4 = base64IndexTable[plainTxt[j++] & 0x3F];
            base64String[loc++] = v1;
            base64String[loc++] = v2;
            base64String[loc++] = v3;
            base64String[loc++] = v4;
        }
        if (penddingBytes == 1)
        {
            auto v1 = base64IndexTable[plainTxt[length - 2] >> 2];
            auto v2 = base64IndexTable[(plainTxt[length - 2] & 0x03) << 4 | plainTxt[length - 1] >> 4];
            auto v3 = base64IndexTable[(plainTxt[length - 1] & 0x0F) << 2];
            base64String[loc++] = v1;
            base64String[loc++] = v2;
            base64String[loc++] = v3;
        }
        else if (penddingBytes == 2)
        {
            auto v1 = base64IndexTable[plainTxt[length - 1] >> 2];
            auto v2 = base64IndexTable[(plainTxt[length - 1] & 0x03) << 4];
            base64String[loc++] = v1;
            base64String[loc++] = v2;
        }

        for (uint8_t i = 0; i < penddingBytes; i++)
        {
            base64String[loc++] = '=';
        }
        return base64String;
    }

    std::string base64Decode(const std::string &base64String)
    {
        auto length = base64String.size();
        decltype(length) loc = 0;
        std::string plainTxt(length / 4 * 3, 0);
        auto calcIdx = [](const unsigned char &c)
        {
            unsigned int idx = 0;
            if (c >= 'A' && c <= 'Z')
                idx = c - 'A';
            else if (c >= 'a' && c <= 'z')
                idx = c - 'a' + 26;
            else if (c >= '0' && c <= '9')
                idx = c - '0' + 52;
            else if (c == '+')
                idx = 62;
            else if (c == '/')
                idx = 63;
            else
                idx = 64;
            return idx;
        };
        for (size_t i = 0, j = 0; i < length / 4; i++)
        {
            auto idx1 = calcIdx(base64String[j++]);
            auto idx2 = calcIdx(base64String[j++]);
            auto idx3 = calcIdx(base64String[j++]);
            auto idx4 = calcIdx(base64String[j++]);

            auto v1 = idx1 << 2 | idx2 >> 4;
            plainTxt[loc++] = v1;
            if (idx3 == 64)
            {
                plainTxt.pop_back();
                plainTxt.pop_back();
                break;
            }

            auto v2 = idx2 << 4 | idx3 >> 2;
            plainTxt[loc++] = v2;
            if (idx4 == 64)
            {
                plainTxt.pop_back();
                break;
            }
            auto v3 = idx3 << 6 | idx4;
            plainTxt[loc++] = v3;
        }
        return plainTxt;
    }

    Interleaved_Rtp rtpOverRtsp(const uint8_t *pkg, std::size_t len)
    {
        int ret = 0;
        if(len < 4 )
        {
            ret = 0;
            return std::make_tuple(ret,0,0,nullptr);
        }
        
        auto channel = pkg[1];
        uint16_t pkgLen = uint16_t(pkg[2]) << 8 | uint16_t(pkg[3]);
        if(pkgLen > len - 4) 
        {
            ret = 0;
            return std::make_tuple(ret,0,0,nullptr);
        }
        ret = pkgLen + 4;
        return std::make_tuple(ret,channel,pkgLen,pkg+4);
    }

    uint64_t parseUTCClock(const std::string& clock)
    {
        struct tm timeinfo;
        sscanf(clock.c_str(),"%04d%02d%02dT%02d%02d%02d",&timeinfo.tm_year,&timeinfo.tm_mon
                                                        ,&timeinfo.tm_mday,&timeinfo.tm_hour
                                                        ,&timeinfo.tm_min,&timeinfo.tm_sec);
        timeinfo.tm_year -= 1900;
        timeinfo.tm_mon -= 1;
        auto sec = mktime(&timeinfo);
        std::string::size_type pos;
        if((pos = clock.find('.')) != std::string::npos)
        {
            auto fraction = clock.substr(pos+1,clock.size() - pos - 2);
            auto millisecond = std::stoi(fraction) / 1000;
            sec = sec * 1000 + millisecond;
        }
        return sec;
    }


    std::string UTCClockToString(uint64_t utc)
    {
        auto fraction = utc % 1000;
        auto sec = utc / 1000;
        auto timeinfo = localtime((time_t*)&sec);
        timeinfo->tm_year += 1900;
        timeinfo->tm_mon += 1;
        std::stringstream ss;
        ss << timeinfo->tm_year << std::setw(2) << std::setfill('0') <<timeinfo->tm_mon << timeinfo->tm_mday <<"T"
           << timeinfo->tm_hour << timeinfo->tm_min << timeinfo->tm_sec;
        if(fraction > 0)
        {
            ss << "." << std::to_string(fraction) << "Z";
        }
        return ss.str();
    }

    uint64_t parseNPTTime(const std::string& npt)
    {
        if(npt.find(":") != std::string::npos)
        {
            int hh = 0, mm = 0;
            float sec = 0.0f;
            sscanf(npt.c_str(),"%d:%d:%f",&hh,&mm,&sec);
            return (hh * 3600 + mm * 60) * 1000 + uint64_t(sec * 1000);
        }
        else
        {
            double sec = std::stof(npt);
            return uint64_t(sec * 1000); 
        }
    }

    std::string NPTTimeToString(uint64_t npt)
    {
        auto fraction = npt % 1000;
        auto sec = npt / 1000;
        std::stringstream ss;
        if(sec > 3600)
        {
            ss << sec / 3600;
            ss << ":" << std::setw(2) << std::setfill('0')  << (sec % 3600 / 60) 
               << ":" << sec % 60 ;
        }
        else
        {
           ss << sec;
        }
        if(fraction > 0)
        {
            ss << "." << std::to_string(fraction);
        }
        return ss.str();
    }

    

    uint8_t S[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
    };

    uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };

    std::string encodeHex(uint32_t n)
    {
        std::stringstream ss;
        char buf[3] = {0};
        sprintf(buf, "%02x", (uint8_t)n);
        ss << std::hex << std::setw(2) << std::setfill('0') << buf;
        sprintf(buf, "%02x", (uint8_t)(n >> 8));
        ss << std::hex << std::setw(2) << std::setfill('0') << buf;
        sprintf(buf, "%02x", (uint8_t)(n >> 16));
        ss << std::hex << std::setw(2) << std::setfill('0') << buf;
        sprintf(buf, "%02x", (uint8_t)(n >> 24));
        ss << std::hex << std::setw(2) << std::setfill('0') << buf;
        return ss.str();
    }

    inline uint32_t leftRoate(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

    void MD5::sum(const std::string& content) { sum(content.data(), content.size()); }

    void MD5::sumFile(const std::string& path)
    {
        std::ifstream in_(path, std::ifstream::binary | std::ifstream::in);
        if (!in_)
        {
            throw std::runtime_error("open file failed");
        }
        char buf[64 * 512] = {0};
        while (in_.read(buf, 64 * 512))
        {
            sum(buf, 64 * 512);
        }
        sum(buf, in_.gcount());
    }
    void MD5::sum(const char* data, std::size_t len)
    {
        const char* message = data;
        uint32_t length = len;

        if (!cache_.empty())
        {
            cache_.append(data, len);
            message = cache_.data();
            length = len;
        }

        total_ += len;
        uint32_t i = 0;
        for (i = 0; i + 64 <= length; i += 64)
        {
            processChunk((const uint8_t*)message + i);
        }
        if (cache_.empty())
        {
            if (i < length)
            {
                cache_.append(message + i, length - i);
            }
        }
        else
        {
            cache_ = cache_.substr(i);
        }
    }
    void MD5::processChunk(const uint8_t* chunk)
    {
        uint32_t A = a_;
        uint32_t B = b_;
        uint32_t C = c_;
        uint32_t D = d_;
        uint32_t M[16] = {0};
        for (int j = 0; j < 16; j++)
        {
            M[j] = chunk[0 + j * 4];
            M[j] |= uint32_t(chunk[1 + j * 4]) << 8;
            M[j] |= uint32_t(chunk[2 + j * 4]) << 16;
            M[j] |= uint32_t(chunk[3 + j * 4]) << 24;
        }

        for (int i = 0; i < 64; i++)
        {
            uint32_t F, g;
            if (i >= 0 && i < 16)
            {
                F = (B & C) | ((~B) & D);
                g = i;
            }
            else if (i >= 16 && i < 32)
            {
                F = (D & B) | ((~D) & C);
                g = (5 * i + 1) % 16;
            }
            else if (i >= 32 && i < 48)
            {
                F = B ^ C ^ D;
                g = (3 * i + 5) % 16;
            }
            else
            {
                F = C ^ (B | (~D));
                g = (7 * i) % 16;
            }
            F = F + A + K[i] + M[g];
            A = D;
            D = C;
            C = B;
            B = B + leftRoate(F, S[i]);
        }
        a_ = a_ + A;
        b_ = b_ + B;
        c_ = c_ + C;
        d_ = d_ + D;
    }
    std::string MD5::final()
    {
        int least = total_ % 64;
        int appendLen = 0;
        if (least < 56)
        {
            appendLen = 56 - least;
        }
        else if (least > 56)
        {
            appendLen = 120 - least;
        }
        if (appendLen > 0)
        {
            cache_.push_back(0x80);
            cache_.append(std::string(appendLen - 1, 0x00));
        }
        total_ = total_ * 8;
        cache_.push_back(uint8_t(total_ & 0xFF));
        cache_.push_back(uint8_t(total_ >> 8 & 0xFF));
        cache_.push_back(uint8_t(total_ >> 16 & 0xFF));
        cache_.push_back(uint8_t(total_ >> 24 & 0xFF));
        cache_.push_back(uint8_t(total_ >> 32 & 0xFF));
        cache_.push_back(uint8_t(total_ >> 40 & 0xFF));
        cache_.push_back(uint8_t(total_ >> 48 & 0xFF));
        cache_.push_back(uint8_t(total_ >> 52 & 0xFF));
        sum(cache_.data(), cache_.size());
        std::string digest = "";
        digest += encodeHex(a_);
        digest += encodeHex(b_);
        digest += encodeHex(c_);
        digest += encodeHex(d_);
        return digest;
    }

    void MD5::reset()
    {
        a_ = 0x67452301;
        b_ = 0xEFCDAB89;
        c_ = 0x98BADCFE;
        d_ = 0x10325476;
        cache_.clear();
        total_ = 0;
    }


}