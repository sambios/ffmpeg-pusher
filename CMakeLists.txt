cmake_minimum_required(VERSION 3.2)
project(ffmpeg_pusher)

set(CMAKE_CXX_STANDARD 14)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(test ffmpeg_push_unittest.cpp)
target_link_libraries(test avfilter avformat avcodec)
