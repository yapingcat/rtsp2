#include "Rtsp2Url.h"
#include "Rtsp2Utility.h"
#include "third/http_parser.h"


namespace rtsp2 {

    int parserUrl(const string &uri,Url& url)
    {
        struct http_parser_url u;
        http_parser_url_init(&u);
        if(http_parser_parse_url(uri.c_str(),uri.size(),false,&u) != 0) {
            return -1;
        }

        if(u.field_set & (1 << UF_USERINFO))
        {
            splitInTwoPart({uri.c_str() + u.field_data[UF_USERINFO].off,u.field_data[UF_USERINFO].len},':',url.username,url.passwd);
        }
        if(u.field_set & (1 << UF_SCHEMA))
        {
            url.scheme = {uri.c_str() + u.field_data[UF_SCHEMA].off,u.field_data[UF_SCHEMA].len};
        }
        if(u.field_set & (1 << UF_HOST))
        {
            url.host = {uri.c_str() + u.field_data[UF_HOST].off,u.field_data[UF_HOST].len};
        }
        if(u.field_set & (1 << UF_PORT))
        {
            url.port = {uri.c_str() + u.field_data[UF_PORT].off,u.field_data[UF_PORT].len};
        }
    
        if(url.port.empty())
		{
			url.port = "554";
		}
        return 0;
    }
}
