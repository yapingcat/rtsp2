#include <iostream>
#include <fstream>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <thread>
#include <cassert>

static const uint8_t *findStartCode(const uint8_t *start, const uint8_t *end)
{
    for (auto p = start; p < end - 4; p++)
    {
        if (p[0] == 0x00 && p[1] == 0x00)
        {
            if (p[2] == 0x01)
            {
                return p;
            }
            else if (p[2] == 0x00 && p[3] == 0x01)
            {
                return p;
            }
        }
    }
    return nullptr;
}

class H264MeidaSource
{
public:
    H264MeidaSource(const std::string &filepath)
    {
        std::ifstream in(filepath);
        assert(in);
        in.seekg(0, in.end);
        auto filesize = in.tellg();
        in.seekg(0, in.beg);
        buf_.resize(filesize);
        in.read((char *)buf_.data(), filesize);
        in.close();
        offset_ = buf_.data();
        end_ = buf_.data() + filesize;
    }


    H264MeidaSource(const H264MeidaSource& other)
		:buf_(other.buf_)
	{
        offset_ = buf_.data();
        end_ = buf_.data() + buf_.size();
	}


    ~H264MeidaSource()
	{
       std::cout<<this<<" destory" <<std::endl;
	}

    std::tuple<char *, int> GetNextFrame()
    {
        if (offset_ == end_)
        {
            return std::make_tuple(nullptr, 0);
        }
        auto start = findStartCode(offset_, end_);
        if (start != nullptr)
        {
            auto next = findStartCode(start + 3, end_);
            if (next != nullptr)
            {
                auto frame = std::make_tuple((char *)start, (int)(next - start));
                offset_ = (uint8_t *)next;
                return frame;
            }
            else
            {
                auto frame = std::make_tuple((char *)start, (int)(end_ - start));
                offset_ = end_;
                return frame;
            }
        }
        else
        {
            return std::make_tuple(nullptr, 0);
        }
    }

private:
    std::vector<uint8_t> buf_;
    uint8_t *offset_;
    uint8_t *end_;
};
