cmake_minimum_required(VERSION 3.21 FATAL_ERROR)
project(webmdx-test LANGUAGES CXX)

find_package(webmdx REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE webmdx::webmdx)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(FILES ${SILO_RUNTIME_LIBRARIES} DESTINATION bin)

set(runtime_dlls $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "$<$<BOOL:${runtime_dlls}>:${CMAKE_COMMAND};-E;copy_if_different;${runtime_dlls};$<TARGET_FILE_DIR:${PROJECT_NAME}>>"
    COMMAND_EXPAND_LISTS)

enable_testing()
set(DOWNLOADED_MEDIA_DIR ${CMAKE_CURRENT_BINARY_DIR}/media)

function(add_decode_test name media_file width height video_packets)
    add_test(NAME decode-${name} COMMAND ${PROJECT_NAME} ${media_file} ${width} ${height} ${video_packets})
endfunction()

function(add_downloaded_decode_test name url sha256 width height video_packets)
    set(media_file ${DOWNLOADED_MEDIA_DIR}/${name}.webm)
    file(DOWNLOAD ${url} ${media_file} EXPECTED_HASH SHA256=${sha256})
    add_decode_test(${name} ${media_file} ${width} ${height} ${video_packets})
endfunction()

# Two seconds of ffmpeg's testsrc2 and a sine tone
add_decode_test(vp8-opus ${CMAKE_CURRENT_LIST_DIR}/media/vp8-opus.webm 320 240 50)
add_decode_test(vp9-opus ${CMAKE_CURRENT_LIST_DIR}/media/vp9-opus.webm 320 240 50)
add_decode_test(av1-opus ${CMAKE_CURRENT_LIST_DIR}/media/av1-opus.webm 320 240 50)

# VP8 video with Vorbis audio, from https://www.webmfiles.org/demo-files/
add_downloaded_decode_test(big-buck-bunny-trailer https://dl6.webmfiles.org/big-buck-bunny_trailer.webm
    519ce67113629c8556337602c28e7dd1286bed58d60936da5dde5215f6427d74 640 360 812)
add_downloaded_decode_test(elephants-dream https://dl6.webmfiles.org/elephants-dream.webm
    b80e6aad4f82e624c0afcc5096dc6922ecc843037025d15047e10c3f575c2d7f 540 360 2822)
