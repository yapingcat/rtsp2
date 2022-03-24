#ifndef RTP_H264_PACKER_H
#define RTP_H264_PACKER_H

#include <cstdint>
#include <functional>

class PackerHandle
{
public:
    void setRtpCallBack(std::function<void(const uint8_t* pkg,std::size_t len)> onRtp)
    {
        onrtp_ = std::move(onRtp);
    }

protected:
    std::function<void(const uint8_t* pkg,std::size_t len)> onrtp_;
};

class H264RtpPacker : public PackerHandle
{
public:
    H264RtpPacker(uint16_t  sequence,uint8_t payload, uint32_t ssrc)
        :seq(sequence),ssrc(ssrc),pt(payload)
    {

    }
    ~H264RtpPacker() = default;

public:
    void pack(const uint8_t* frame,std::size_t len,uint32_t timestamp);

private:
    uint16_t seq = 0;
    uint32_t ssrc;
    uint8_t pt = 96;
};



#endif

