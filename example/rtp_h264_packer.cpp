#include "rtp_h264_packer.h"
#include "RtpPacket.h"

#include <algorithm>
#include <iostream>


const uint8_t* removeStartCode(const uint8_t* frame,std::size_t& len)
{
    if(frame[0] == 0x00 && frame[1] == 0x00)
    {
        if(frame[2] == 0x01)
        {
            len -= 3;
            return frame + 3;
        }
        else if(frame[2] == 0x00 && frame[3] == 0x01)
        {
             len -= 4;
            return frame + 4;
        }
    }
    return frame;
}

void H264RtpPacker::pack(const uint8_t* frame,std::size_t len,uint32_t timestamp)
{
    uint8_t rtp[1500];
    const uint8_t* p = removeStartCode(frame,len);
    int start = 1;
    uint8_t FUindicator = (p[0] & 0xE0) | 0x1C;
    uint8_t FUheader  = p[0] & 0x1F;
    while(len > 0)
    {
        uint8_t marker = 0;
        rtp[0] = (0x02 << 6);
        rtp[1] = (marker << 7) | pt;
        rtp[2] = (uint8_t)((seq >> 8) & 0xFF);
        rtp[3] = (uint8_t)(seq & 0xFF);
        rtp[4] = (uint8_t)((timestamp >> 24) & 0xFF);
        rtp[5] = (uint8_t)((timestamp >> 16) & 0xFF);
        rtp[6] = (uint8_t)((timestamp>> 8) & 0xFF);
        rtp[7] = (uint8_t)(timestamp & 0xFF);
        rtp[8] = (uint8_t)((ssrc >> 24) & 0xFF);
        rtp[9] = (uint8_t)((ssrc >> 16) & 0xFF);
        rtp[10] = (uint8_t)((ssrc >> 8) & 0xFF);
        rtp[11] = (uint8_t)(ssrc & 0xFF);
        seq++;
        if(len <= 1400)
        {   
            marker = 1;
            rtp[1] = (marker << 7) | pt;
            if(start)
            {   
                std::copy(p,p+len,rtp+ 12);
                onrtp_(rtp,12+len);
                return;
            }
            else
            {
                FUheader = (FUheader & 0x3F) | 0x40;
                rtp[12] = FUindicator;
                rtp[13] = FUheader;
                std::copy(p,p+len,rtp + 14);
                onrtp_(rtp,14+len);
                return;
            }
        }
        rtp[1] = (marker << 7) | pt;
        if(start)
        {
            FUheader = (FUheader & 0x3F) | 0x80;    
            p++;
            len--;
            start = 0;
        }
        else
        {
            FUheader = (FUheader & 0x3F);
        }
        rtp[12] = FUindicator;
        rtp[13] = FUheader;
        std::copy(p,p+1400,rtp + 14);
        onrtp_(rtp,14+1400);
        len -= 1400;
        p += 1400;
    }
}


