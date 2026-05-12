add_executable(transcodeToAAC transcodeToAAC.cpp)

target_link_libraries(transcodeToAAC FFmpegWrapper)

add_custom_command(
    TARGET transcodeToAAC
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/FurElise.ogg"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/FurElise.ogg"
    COMMENT "Copying FurElise.ogg to output directory"
)
