#include "RtpPacket.h"


std::size_t calcRtpHeaderLen(const RtpPacket& pkg)
{
    if(!pkg)
        return 0;
    
    std::size_t hdrlen = RtpPacket::RTP_FIX_HEAD;
    hdrlen += pkg.csrccount * 4;
    hdrlen += pkg.extension ? pkg.extensions.size() + 4 : 0;
    return hdrlen;
}

RtpPacket decode(uint8_t* packet,std::size_t len)
{
    RtpPacket pkg;
    if(len < RtpPacket::RTP_FIX_HEAD)
    {
        return pkg;   
    }
    
    pkg.version      = (packet[0] & 0xC0) >> 6;
    pkg.padding      = (packet[0] & 0x20) >> 5;
    pkg.extension    = (packet[0] & 0x10) >> 4;
    pkg.csrccount    = (packet[0] & 0x0F);
    pkg.marker       = (packet[1] & 0x80) >> 7;
    pkg.pt           = (packet[1] & 0x7F);
    pkg.sequence     = ((uint16_t)(packet[2]) << 8) | (uint16_t)(packet[3]);
    pkg.timeStamp    = ((uint32_t)(packet[4]) << 24) | ((uint32_t)(packet[5]) << 16) | ((uint32_t)(packet[6]) << 8) | ((uint32_t)(packet[7]));
    pkg.ssrc         = ((uint32_t)(packet[8]) << 24) | ((uint32_t)(packet[9]) << 16) | ((uint32_t)(packet[10]) << 8) | ((uint32_t)(packet[11]));

    std::size_t hdrlen = RtpPacket::RTP_FIX_HEAD;
    if(pkg.csrccount > 0)
    {
        if(4 * pkg.csrccount + RtpPacket::RTP_FIX_HEAD > len  )
        {
            return pkg;
        }
        pkg.csrcs.reserve(pkg.csrccount);
        for(int i = 0; i < pkg.csrccount; i++)
        {
            pkg.csrcs[i] = ((uint32_t)(packet[12 + i]) << 24) 
                            | ((uint32_t)(packet[13 + i]) << 16) 
                                | ((uint32_t)(packet[14 + i]) << 8) 
                                    | ((uint32_t)(packet[15 + i]));
        }
        hdrlen += pkg.csrccount * 4;
    }
    if(pkg.extension)
    {
        if(hdrlen + 4 > len )
        {
            return pkg;
        }
        uint8_t* ext = packet + RtpPacket::RTP_FIX_HEAD + pkg.csrccount * 4;
        pkg.profile[0] = ext[0];
        pkg.profile[1] = ext[1];
        uint16_t extLength = (uint16_t)ext[2] << 8 | (uint16_t)ext[3];
        if(hdrlen + 4 + extLength * 4 > len)
            return pkg;
        pkg.extensions.reserve(extLength * 4);
        pkg.extensions.insert(pkg.extensions.end(),ext + 4, ext + 4 + extLength * 4);
        hdrlen += 4 + extLength * 4;
    }

    uint8_t paddingCount = 0;
    if(pkg.padding)
    {
        if(hdrlen + packet[len - 1] > len)
        {
            return pkg;
        }
        paddingCount = packet[len - 1];
        pkg.paddings.insert(pkg.paddings.end(),packet + len - paddingCount,packet + len);
    }
    pkg.payload.reserve(len - hdrlen - paddingCount);
    pkg.payload.insert(pkg.payload.end(),packet + hdrlen, packet + len - paddingCount);
    return pkg;
}

std::vector<uint8_t> encode(const RtpPacket& pkg)
{
    if(!pkg)
        return {};
    
    int rtpLen = calcRtpHeaderLen(pkg) + pkg.payload.size() + pkg.paddings.size();
    std::vector<uint8_t> rtp;
    rtp.reserve(rtpLen);
    rtp.resize(RtpPacket::RTP_FIX_HEAD);
    rtp[0] = (pkg.version << 6) | (pkg.padding << 5) | (pkg.extension << 4) | pkg.csrccount;
    rtp[1] = (pkg.marker << 7) | pkg.pt;
    rtp[2] = (uint8_t)((pkg.sequence >> 8) & 0xFF);
    rtp[3] = (uint8_t)(pkg.sequence & 0xFF);
    rtp[4] = (uint8_t)((pkg.timeStamp >> 24) & 0xFF);
    rtp[5] = (uint8_t)((pkg.timeStamp >> 16) & 0xFF);
    rtp[6] = (uint8_t)((pkg.timeStamp >> 8) & 0xFF);
    rtp[7] = (uint8_t)(pkg.timeStamp & 0xFF);
    rtp[8] = (uint8_t)((pkg.ssrc >> 24) & 0xFF);
    rtp[9] = (uint8_t)((pkg.ssrc >> 16) & 0xFF);
    rtp[10] = (uint8_t)((pkg.ssrc >> 8) & 0xFF);
    rtp[11] = (uint8_t)(pkg.ssrc & 0xFF);
    
    if(pkg.csrccount > 0)
        rtp.insert(rtp.end(),pkg.csrcs.begin(),pkg.csrcs.end());
    if(pkg.extension)
    {
        rtp.insert(rtp.end(),pkg.profile.begin(),pkg.profile.end());
        uint16_t extLength = (uint16_t)pkg.extensions.size();
        rtp.push_back((uint8_t)((extLength >> 8) & 0xFF));
        rtp.push_back((uint8_t)(extLength & 0xFF));
        rtp.insert(rtp.end(),pkg.extensions.begin(),pkg.extensions.end());
    }
    rtp.insert(rtp.end(),pkg.payload.begin(),pkg.payload.end());
    if(pkg.padding)
    {
        rtp.insert(rtp.end(),pkg.paddings.begin(),pkg.paddings.end());
        rtp.push_back((uint8_t)(pkg.paddings.size()));
    }
    return rtp;
}


