cmake_minimum_required(VERSION 3.0)
#-fsanitize=address
project(rtsp2)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -fPIC -std=c++11")

set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb3")
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -DNDEBUG")

option(BUILD_EXAMPLE "build example" OFF)
option(ASAN  "enable asan" OFF)


if (ASAN)
    if (UNIX AND NOT APPLE)
        set(CMAKE_BUILD_TYPE DEBUG)
        message(STATUS "enable ASAN") 
        add_definitions(-fsanitize=address -fno-omit-frame-pointer)
    else()
        message(STATUS "enable ASAN Failed") 
    endif()
endif()

if(CMAKE_BUILD_TYPE)
else()
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

message(STATUS "BuildType ${CMAKE_BUILD_TYPE}")

add_custom_target(debug
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=DEBUG ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Switch CMAKE_BUILD_TYPE to Debug"
)

add_custom_target(release
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=RELEASE ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Switch CMAKE_BUILD_TYPE to Release"
)

add_custom_target(example
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -DBUILD_EXAMPLE=ON -DCMAKE_BUILD_TYPE=DEBUG ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Switch to enable example compile"
)

add_custom_target(asan
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -DASAN=ON -DCMAKE_BUILD_TYPE=DEBUG ${CMAKE_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Switch to enable asan compile"
)

aux_source_directory(src/  SOURCE)
set(HTTP_SOURCE http-parser/http_parser.c)
set(LIBRTSP2_VERSION  1.0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(rtsp2_static STATIC ${SOURCE} ${HTTP_SOURCE})
add_library(rtsp2_shared SHARED ${SOURCE} ${HTTP_SOURCE})

set_target_properties(rtsp2_static PROPERTIES OUTPUT_NAME rtsp2)
set_target_properties(rtsp2_shared PROPERTIES OUTPUT_NAME rtsp2)

set_target_properties(rtsp2_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
set_target_properties(rtsp2_shared PROPERTIES CLEAN_DIRECT_OUTPUT 1)

set_target_properties(rtsp2_shared PROPERTIES VERSION ${LIBRTSP2_VERSION} SOVERSION ${LIBRTSP2_VERSION})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION "include/librtsp2")
install(TARGETS rtsp2_shared LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS rtsp2_static ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if (BUILD_EXAMPLE)
    add_subdirectory(example)
endif()




