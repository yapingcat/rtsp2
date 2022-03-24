cmake_minimum_required(VERSION 3.0)
#-fsanitize=address
project(rtsp2)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -fno-omit-frame-pointer  -fsanitize=address -g -Wall -fPIC -std=c++11")

aux_source_directory(src/  SOURCE)
aux_source_directory(third/  3rd)
set(LIBRTSP2_VERSION  1.0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(rtsp2_static STATIC ${SOURCE} ${3rd})
add_library(rtsp2_shared SHARED ${SOURCE} ${3rd})

set_target_properties(rtsp2_static PROPERTIES OUTPUT_NAME rtsp2)
set_target_properties(rtsp2_shared PROPERTIES OUTPUT_NAME rtsp2)

set_target_properties(rtsp2_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(rtsp2_shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)

set_target_properties(rtsp2_shared PROPERTIES VERSION ${LIBRTSP2_VERSION} SOVERSION ${LIBRTSP2_VERSION})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION "include/librtsp2")
install(TARGETS rtsp2_shared LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS rtsp2_static ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})


add_subdirectory(example)


