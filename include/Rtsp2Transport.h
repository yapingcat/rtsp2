#ifndef RTSP2_TRANSPORT_H
#define RTSP2_TRANSPORT_H

#include <string>

namespace rtsp2
{

    enum LowerTransport
    {
        TCP = 0,
        UDP = 1,
    };

    enum CastType
    {
        UNICAST = 0,
        MULTICAST = 1,
    };

    enum MODE
    {
        PLAY = 0,
        RECORD = 1,
    };

    struct TcpParam
    {
        int interleaved[2] = {-1, -1};
        std::string toString() const;
    };

    struct UdpParam
    {
        int client_port[2] = {-1, -1};
        int server_port[2] = {-1, -1};
        std::string toString() const;
    };
    struct MulticastParam
    {
        std::string destination = "";
        int ttl = -1;
        int port = -1;
        std::string toString() const;
    };

    struct Transport
    {
        int version = 1;
        virtual ~Transport() = default;
    };

    struct TransportV1 : public Transport
    {
        LowerTransport proto;
        CastType cast;
        MODE mode;
        TcpParam tcpParam;
        UdpParam udpParam;
        MulticastParam multicastParam;
        std::string ssrc;
        std::string toString() const;
    };

    TransportV1 decodeTransportV1(const std::string &transport);
    //TransportV2 decodeTransportV2(const std::string &transport);

} // namespace rtsp2

#endif