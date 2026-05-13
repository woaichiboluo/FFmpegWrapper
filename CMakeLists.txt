cmake_minimum_required(VERSION 3.10)

project(FFmpegWrapper)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FFMPEG_INCLUDE_DIRS "D:/media-autobuild_suite-master/local64/bin-video/ffmpegSHARED/include")
set(FFMPEG_LIBRARIES "D:/media-autobuild_suite-master/local64/bin-video/ffmpegSHARED/bin")

add_compile_options(
  /utf-8 /wd4275
  $<$<CXX_COMPILER_ID:MSVC>:/W3>
  $<$<CXX_COMPILER_ID:MSVC>:/WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Werror>
  -D_CRT_SECURE_NO_WARNINGS
)

include_directories(${FFMPEG_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(FFmpegWrapper SHARED 
    src/Common.cpp
    src/AVFormatContextWrap.cpp
    src/AVPacketWrap.cpp
    src/AVCodecContextWrap.cpp
    src/AVFrameWrap.cpp
  src/AVAudioFifoWrap.cpp
)

target_link_libraries(FFmpegWrapper ${FFMPEG_LIBRARIES}/avdevice.lib 
                                    ${FFMPEG_LIBRARIES}/avutil.lib
                                    ${FFMPEG_LIBRARIES}/avformat.lib
                                    ${FFMPEG_LIBRARIES}/avcodec.lib
                                    ${FFMPEG_LIBRARIES}/swscale.lib
                                    ${FFMPEG_LIBRARIES}/swresample.lib
                                    ${FFMPEG_LIBRARIES}/avfilter.lib
                                    )

message(STATUS "FFmpeg include directories: ${FFMPEG_INCLUDE_DIRS}")
message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}/avdevice.lib")

add_subdirectory(examples)