#ifndef RTPPACKET_H
#define RTPPACKET_H

#include <cstdint>
#include <vector>
#include <array>

struct RtpPacket
{   
    enum { RTP_FIX_HEAD = 12 };

    uint8_t version =  0;
    uint8_t padding = 0;
    uint8_t extension = 0;
    uint8_t csrccount = 0;
    uint8_t marker = 0;
    uint8_t pt = 0;
    uint16_t sequence = 0;
    uint32_t timeStamp = 0;
    uint32_t ssrc = 0;

    std::array<uint8_t,2> profile;
    std::vector<uint32_t> csrcs;
    std::vector<uint8_t> extensions; 
    std::vector<uint8_t> payload;
    std::vector<uint8_t> paddings;
    explicit operator bool() const
    {
        return version == 2 && pt < 127
                && (csrcs.size() == csrccount && csrccount < 16 )
                    && (!extension || (extensions.size() > 0 && extensions.size() % 4 == 0))
                        && (!padding || paddings.size() > 0);
    }
};

std::size_t calcRtpHeaderLen(const RtpPacket& pkg);

RtpPacket decode(uint8_t* packet,std::size_t len);

std::vector<uint8_t> encode(const RtpPacket& pkg);




#endif
