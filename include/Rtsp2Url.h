#ifndef RTSP2_URL_H
#define RTSP2_URL_H

#include <string>
#include "Rtsp2Response.h"

using std::string;

namespace rtsp2
{

    struct Url
    {
        string scheme="";
        string username="";
        string passwd="";
        string host = "";
        string port = "";
    };

    int parserUrl(const string &uri,Url& url);


} // namespace rtsp2

#endif