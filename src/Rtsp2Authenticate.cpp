#include <sstream>
#include <iostream>
#include <chrono>

#include "Rtsp2Utility.h"
#include "Rtsp2Authenticate.h"
#include "Rtsp2Utility.h"

namespace rtsp2
{
    std::string Authenticate::AUTH_DIGEST = "Digest";
    std::string Authenticate::AUTH_BASIC = "Basic";
    std::atomic<int> Authenticate::nonceCounter(0);
    int Authenticate::parser(const std::string &auth)
    {                                                                           
        auto tmp = trimSpace(auth);
        if(startWithString(tmp,AUTH_BASIC))
        {
            scheme = AUTH_BASIC;
            response = trimSpace(tmp.substr(5));
            return 0;
        }
        scheme = AUTH_DIGEST;
        tmp = tmp.substr(7);
        auto strs = splitString(tmp,',');
        for(auto && s : strs)
        {
            s = trimSpace(s);
            std::string key;
            std::string value;
            splitInTwoPart(s,'=',key,value);
            if(value.size() >= 2)
                value = value.substr(1,value.size()-2);
            if(key == "realm")
                realm = value;
            else if(key == "nonce")
                nonce = value;
            else if(key == "opaque")
                opaque = value;
            else if(key == "username")
                username = value;
            else if(key == "uri")
                uri = value;
            else if(key == "response")
                response = value;
        }
        return 0;
    }

    std::string Authenticate::genAuthInfo()
    {
        std::stringstream ss;
        ss << scheme << " " << "realm=\"" << realm << "\"";
        if(scheme == AUTH_BASIC)
        {
            return ss.str();
        }
        ss << ",nonce=\"" << nonce << "\"";           
        if(!opaque.empty())
        {
            ss << ",opaque=\""<<opaque<<"\"";
        }
        return ss.str();
    }

    std::string Authenticate::createNonce()
    {
        int nonce = nonceCounter.fetch_add(1);
        auto n =  std::chrono::system_clock::now();
        auto tv = n.time_since_epoch().count();
        MD5 md5digest;
        md5digest.sum((char*)&nonce,sizeof(nonce));
        md5digest.sum((char*)&tv,sizeof(tv));
        return md5digest.final();
    }

    std::string Authenticate::credentials()
    {
        if (scheme == AUTH_BASIC)
        {
            return basicCredentials();
        }
        else
        {
            return digestCredentials();
        }
    }

    std::string Authenticate::basicCredentials()
    {
        std::stringstream ss;
        ss << scheme << " " << base64Encode(username + ":" + password);
        return ss.str();
    }

    std::string Authenticate::digestCredentials()
    {
        std::stringstream ss;
        ss << scheme << " "
           << "username=\"" << username << "\", nonce=\""
           << nonce << "\", uri=\"" << uri << "\", response=\"" << makeResponse() << "\"";
        if (!opaque.empty())
        {
            ss << ", opaque=\"" << opaque << "\"";
        }
        return ss.str();
    }

    //response=md5(md5(username:realm:password):nonce:md5(method:url));
    std::string Authenticate::makeResponse()
    {
        MD5 md5;
        md5.sum(username + ":" + realm + ":" + password);
        auto A1 = md5.final();
        md5.reset();
        md5.sum(method + ":" + uri);
        auto A2 = md5.final();
        md5.reset();
        md5.sum(A1 + ":" + nonce +  ":" + A2);
        return md5.final();
    }


}

#ifdef AUTH_TEST
#include <iostream>
#include <cassert>
using namespace rtsp2;
int main()
{
    MD5 md5;
    md5.sum("test:rtsp2-server:123456");
    auto a1 = md5.final();
    std::cout<<md5.final() <<std::endl;
    MD5 md52;
    md52.sum("test:rtsp2-server:123456");
   
    auto a2 = md52.final();
    std::cout<<md52.final() <<std::endl;
    assert(a1 == a2);
    Authenticate auth;
    auth.username = "test";
    auth.passward = "123456";
    auth.nonce = "19dfe5aa12c0ca65ebc0ed7b57b7e921";
    auth.uri = "rtsp://49.235.110.177:15555/test.h264";
    auth.method = "OPTIONS";
    auth.realm = "rtsp2-server";
    std::cout<<auth.makeResponse() <<std::endl;
}


#endif