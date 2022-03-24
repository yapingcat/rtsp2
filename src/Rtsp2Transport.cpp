#include <cstring>
#include <sstream>

#include "Rtsp2Transport.h"
#include "Rtsp2Utility.h"

namespace rtsp2
{
    //    Transport           =    "Transport" ":"
    //                             1\#transport-spec
    //    transport-spec      =    transport-protocol/profile[/lower-transport]
    //                             *parameter
    //    transport-protocol  =    "RTP"
    //    profile             =    "AVP"
    //    lower-transport     =    "TCP" | "UDP"
    //    parameter           =    ( "unicast" | "multicast" )
    //                        |    ";" "destination" [ "=" address ]
    //                        |    ";" "interleaved" "=" channel [ "-" channel ]
    //                        |    ";" "append"
    //                        |    ";" "ttl" "=" ttl
    //                        |    ";" "layers" "=" 1*DIGIT
    //                        |    ";" "port" "=" port [ "-" port ]
    //                        |    ";" "client_port" "=" port [ "-" port ]
    //                        |    ";" "server_port" "=" port [ "-" port ]
    //                        |    ";" "ssrc" "=" ssrc
    //                        |    ";" "mode" = <"> 1\#mode <">
    //    ttl                 =    1*3(DIGIT)
    //    port                =    1*5(DIGIT)
    //    ssrc                =    8*8(HEX)
    //    channel             =    1*3(DIGIT)
    //    address             =    host
    //    mode                =    <"> *Method <"> | Method 


    //    Example:
    //      Transport: RTP/AVP;multicast;ttl=127;mode="PLAY",
    //                 RTP/AVP;unicast;client_port=3456-3457;mode="PLAY"
    TransportV1 decodeTransportV1(const std::string &transport)
    {
        TransportV1 trans;
        auto parameters = splitString(transport, ';');
        for(auto && param : parameters) 
        {
            if(startWithString(param,"RTP/AVP"))
            {
                if(param == "RTP/AVP/TCP")
                {
                    trans.proto = TCP;
                }
                else 
                {
                    trans.proto = UDP;
                }
            }
            else if (startWithString(param,"multicast"))
            {
                trans.cast = MULTICAST;
            }
            else if (startWithString(param,"unicast"))
            {
                trans.cast = UNICAST;
            }
            else if (startWithString(param,"ttl"))
            {
                sscanf(param.c_str(),"ttl=%d",&(trans.multicastParam.ttl));
            }
            else if (startWithString(param,"mode"))
            {
                char mode[16] = {0};
                sscanf(param.c_str(),"mode=%s",mode);
                if(memcpy(mode,"PLAY",4) == 0)
                {
                    trans.mode = PLAY;
                }
                else if(memcpy(mode,"RECORD",6) == 0)
                {
                    trans.mode = RECORD;
                }
            }
            else if (startWithString(param,"client_port"))
            {
                sscanf(param.c_str(),"client_port=%d-%d",trans.udpParam.client_port,trans.udpParam.client_port+1);
            }
            else if (startWithString(param,"server_port"))
            {
                sscanf(param.c_str(),"server_port=%d-%d",trans.udpParam.server_port,trans.udpParam.server_port+1);
            }
            else if (startWithString(param,"interleaved"))
            {
                sscanf(param.c_str(),"interleaved=%d-%d",trans.tcpParam.interleaved,trans.tcpParam.interleaved+1);
            }
            else if (startWithString(param,"ssrc"))
            {
                char ssrcbuf[32] = {0};
                sscanf(param.c_str(),"ssrc=%s",ssrcbuf);
                trans.ssrc = ssrcbuf;
            }
            else if (startWithString(param,"destination"))
            {
                char addr[64] = {0};
                sscanf(param.c_str(),"destination=%s",addr);
                trans.multicastParam.destination = addr;
            }
            else if (startWithString(param,"port"))
            {
                sscanf(param.c_str(),"port=%d",&(trans.multicastParam.port));
            }
        }
        return trans;
    }


    std::string TcpParam::toString() const 
    {
        std::stringstream ss;
        ss << ";interleaved=" << interleaved[0] <<"-"<<interleaved[1];
        return ss.str();
    }

    std::string UdpParam::toString() const
    {
        std::stringstream ss;
        if(client_port[0] > 0)
        {
            ss << ";client_port=" << client_port[0] << "-" << client_port[1];
        }
        if(server_port[0] > 0)
        {
            ss << ";server_port=" << server_port[0] << "-" << server_port[1];
        }
        return ss.str();
    }

    std::string MulticastParam::toString() const
    {
        std::stringstream ss;
        if(ttl > 0)
        {
            ss << ";ttl=" << ttl;
        }
        if(port > 0)
        {
            ss << ";port=" << port;
        }
        if(!destination.empty())
        {
            ss <<";destination" << destination;
        }
        return ss.str();
    }

    std::string TransportV1::toString() const
    {
        std::stringstream ss;
        ss << "RTP/AVP";
        if(proto == TCP)
        {
            ss << "/TCP";
            ss << tcpParam.toString();
        }
        else
        {
            ss << "/UDP";
            ss << udpParam.toString();
        }
        
        if(cast == MULTICAST)
        {
            ss <<";multicast";
            ss << multicastParam.toString();
        }
        else
        {
            ss << ";unicast";
        }

        if(mode == RECORD)
        {
            ss << ";mode=\"RECORD\"";
        }
        else
        {
            ss << ";mode=\"PLAY\"";
        }
        
        if(!ssrc.empty())
        {
            ss << ";ssrc=" << ssrc;
        }
        return ss.str();
    }
}

#ifdef TRANSPORT_TEST 

#include <iostream>

int main()
{
    rtsp2::TransportV1 trans;
    trans.proto = rtsp2::TCP;
    trans.cast = rtsp2::UNICAST;
    trans.tcpParam.interleaved[0] = 0;
    trans.tcpParam.interleaved[1] = 1;
    trans.mode = rtsp2::PLAY;
    trans.ssrc = "12345678";
    std::cout<<trans.toString() <<std::endl;
}


#endif

