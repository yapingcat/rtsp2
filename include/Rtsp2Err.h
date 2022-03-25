#ifndef RTSP2_ERR_H
#define RTSP2_ERR_H

#include <system_error>


namespace rtsp2
{
    enum class RTSP2_ERROR : int
    {
        rtsp2_ok = 0,
        rtsp2_msg_parser_failed = 200,
        rtsp2_rtp_parser_failed = 201,
        rtsp2_session_id_error = 203,
        rtsp2_media_not_exist = 204,
        rtsp2_need_transport = 205,
        rtsp2_pipeline_empty = 206,
        rtsp2_pipeline_not_found = 207,
    };

    std::error_code makeError(RTSP2_ERROR err);
}





#endif
