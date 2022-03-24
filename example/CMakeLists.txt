set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fno-omit-frame-pointer -fsanitize=address -g -ggdb3")
#-fsanitize=address
include_directories("/usr/local/include/asio/include")

add_executable(rtsp_v1_client_use_libuv rtsp_v1_client_use_libuv.cpp)
target_link_libraries(rtsp_v1_client_use_libuv asan rtsp2_static uv_a pthread dl )

add_executable(rtsp_v1_server_use_libuv rtsp_v1_server_use_libuv.cpp rtp_h264_packer.cpp)
target_link_libraries(rtsp_v1_server_use_libuv rtsp2_static uv_a pthread dl )

add_executable(rtsp_v1_client_push_asio rtsp_v1_client_push_asio.cpp rtp_h264_packer.cpp)
target_link_libraries(rtsp_v1_client_push_asio rtsp2_static uv_a pthread dl )


add_executable(rtsp_v1_push_server_asio rtsp_v1_push_server_asio.cpp)
target_link_libraries(rtsp_v1_push_server_asio rtsp2_static uv_a pthread dl )

