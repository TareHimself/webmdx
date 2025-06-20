cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

project(webmdx LANGUAGES C CXX VERSION 1.0.0 DESCRIPTION "C++ library for decoding of webm files")

option(WEBM_DX_BUILD_TEST_MAIN "Build a test main file for debugging" OFF)

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/lib/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/lib/*.h")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

#find_package(PkgConfig REQUIRED)
find_package(unofficial-libwebm CONFIG  REQUIRED)
find_package(libyuv CONFIG  REQUIRED)

#target_include_directories(${PROJECT_NAME} PRIVATE ${Webm_INCLUDE_DIRS} ${Yuv_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} unofficial::libwebm::libwebm yuv)

#pkg_check_modules(VPX vpx)
find_package(vpx)
if(vpx_FOUND)
    message(STATUS "Vpx Enabled")
    target_include_directories(${PROJECT_NAME} PRIVATE ${vpx_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${vpx_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_CODEC_VIDEO_VPX)
else()
    message(STATUS "Vpx Disabled")
endif()

find_package(dav1d)
if(dav1d_FOUND)
    message(STATUS "Av1 Enabled")
    target_include_directories(${PROJECT_NAME} PRIVATE ${dav1d_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${dav1d_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_CODEC_VIDEO_DAV1D)
else()
    message(STATUS "Av1 Disabled")
endif()

find_package(Opus CONFIG)
if(Opus_FOUND)
    message(STATUS "Opus Enabled")
    #target_include_directories(${PROJECT_NAME} PRIVATE ${Opus_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} Opus::opus)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_CODEC_AUDIO_OPUS)
else()
    message(STATUS "Opus Disabled")
endif()

#find_package(Vorbis)
if(Vorbis_FOUND)
    message(STATUS "Vorbis Enabled")
    target_include_directories(${PROJECT_NAME} PRIVATE $Vorbis_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Vorbis_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_CODEC_AUDIO_VORBIS)
else()
    message(STATUS "Vorbis Disabled")
endif()

if(WEBM_DX_BUILD_TEST_MAIN)
    set(STB_IMAGE_WRITE_DIR ${CMAKE_CURRENT_BINARY_DIR}/stb)
    set(STB_IMAGE_WRITE_FILE ${STB_IMAGE_WRITE_DIR}/stb_image_write.h)
    if(NOT EXISTS ${STB_IMAGE_WRITE_FILE})
        file(MAKE_DIRECTORY ${STB_IMAGE_WRITE_DIR})
        file(DOWNLOAD  "https://raw.githubusercontent.com/nothings/stb/refs/heads/master/stb_image_write.h" ${STB_IMAGE_WRITE_FILE} SHOW_PROGRESS)
    endif()
    add_executable(test ${CMAKE_CURRENT_LIST_DIR}/test/main.cpp)
    target_link_libraries(test ${PROJECT_NAME})
    target_include_directories(test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src ${STB_IMAGE_WRITE_DIR})
endif()

install(
        DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
        DESTINATION include/
)

install(
        TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

install(
        EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Config.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION lib/cmake/${PROJECT_NAME}
)


