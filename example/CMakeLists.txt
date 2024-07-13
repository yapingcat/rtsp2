set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}   -g -ggdb3")
#-fno-omit-frame-pointer -fsanitize=address
include_directories("/usr/local/include/asio/include")

if(APPLE)
include_directories("/usr/local/include/")
include_directories("/Users/caoyaping/Downloads/asio-1.20.0/include")
link_directories("/usr/local/lib/")
endif()

if(ASAN)
    set(LINK_LIB asan rtsp2_static uv pthread dl)    
else()
    set(LINK_LIB rtsp2_static uv pthread dl)  
endif()

add_executable(rtsp_v1_client_use_libuv rtsp_v1_client_use_libuv.cpp)
target_link_libraries(rtsp_v1_client_use_libuv  ${LINK_LIB})

add_executable(rtsp_v1_server_use_libuv rtsp_v1_server_use_libuv.cpp rtp_h264_packer.cpp)
target_link_libraries(rtsp_v1_server_use_libuv ${LINK_LIB} )

add_executable(rtsp_v1_client_push_asio rtsp_v1_client_push_asio.cpp rtp_h264_packer.cpp)
target_link_libraries(rtsp_v1_client_push_asio ${LINK_LIB} )

add_executable(rtsp_v1_push_server_asio rtsp_v1_push_server_asio.cpp)
target_link_libraries(rtsp_v1_push_server_asio ${LINK_LIB} )

add_executable(rtsp_v1_client_pipiline_asio rtsp_v1_client_pipiline_asio.cpp)
target_link_libraries(rtsp_v1_client_pipiline_asio ${LINK_LIB} )

