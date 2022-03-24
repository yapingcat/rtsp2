#ifndef RTSP2_AUTHENICATE_H
#define RTSP2_AUTHENICATE_H

#include <string>
#include <atomic>

namespace rtsp2
{
    struct Authenticate
    {
        static std::string AUTH_DIGEST;
        static std::string AUTH_BASIC;
        static std::atomic<int> nonceCounter;
        std::string scheme = "";
        std::string username = "";
        std::string password = "";
        std::string realm = "";
        std::string nonce = "";
        std::string domain = "";
        std::string opaque = "";
        std::string uri = "";
        std::string method = "";
        std::string response = "";  
        int parser(const std::string &auth);
        std::string genAuthInfo();
        static std::string createNonce();
        std::string credentials();
        std::string basicCredentials();
        std::string digestCredentials();
        std::string makeResponse();
    };

}

#endif
