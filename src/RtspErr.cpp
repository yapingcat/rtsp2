#include "Rtsp2Err.h"
#include "http-parser/http_parser.h"

namespace rtsp2
{
    class rtsp_error_category : public std::error_category
    {
    public:
        virtual const char *name() const throw()
        {
            return "rtsp_error";
        }

        virtual std::string message(int v) const
        {
            switch (v)
            {
            case (int)RTSP2_ERROR::rtsp2_ok:
                return "ok";
            case (int)RTSP2_ERROR::rtsp2_msg_parser_failed:
                return "parser rtsp message failed";
            case (int)RTSP2_ERROR::rtsp2_rtp_parser_failed:
                return "parser rtp failed";
            case (int)RTSP2_ERROR::rtsp2_session_id_error:
                return "rtsp session id is wrong";
            case (int)RTSP2_ERROR::rtsp2_media_not_exist:
                return "media is not exist";
            }
            return "";
        }
    };

    const std::error_category &GetVAErrorCategory()
    {
        static rtsp_error_category obj;
        return obj;
    }

    std::error_code makeError(RTSP2_ERROR err)
    {
        return std::error_code(int(err), GetVAErrorCategory());
    }
}