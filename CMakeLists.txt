cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

project(Webmd LANGUAGES C CXX VERSION 1.0.0 DESCRIPTION "C++ library for decoding of Webm files")

option(WEBMD_BUILD_TEST_MAIN "Build a test main file for debugging" OFF)

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/lib/*.cpp")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

find_package(Webm REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${Webm_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Webm_LIBRARIES})

find_package(Vpx)
if(Vpx_FOUND)
    message(STATUS "Vpx Enabled")
    target_include_directories(${PROJECT_NAME} PRIVATE ${Vpx_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Vpx_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBMD_CODEC_VIDEO_VPX)
else()
    message(STATUS "Vpx Disabled")
endif()

#find_package(Dav1d)
if(Dav1d_FOUND)
    message(STATUS "Av1 Enabled")
    target_include_directories(${PROJECT_NAME} PRIVATE ${Dav1d_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Dav1d_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBMD_CODEC_VIDEO_DAV1D)
else()
    message(STATUS "Av1 Disabled")
endif()

find_package(Opus)
if(Opus_FOUND)
    message(STATUS "Opus Enabled")
    target_include_directories(${PROJECT_NAME} PRIVATE ${Opus_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Opus_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBMD_CODEC_AUDIO_OPUS)
else()
    message(STATUS "Opus Disabled")
endif()

#find_package(Vorbis)
if(Vorbis_FOUND)
    message(STATUS "Vorbis Enabled")
    target_include_directories(${PROJECT_NAME} PRIVATE $Vorbis_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${Vorbis_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBMD_CODEC_AUDIO_VORBIS)
else()
    message(STATUS "Vorbis Disabled")
endif()

if(WEBMD_BUILD_TEST_MAIN)
    add_executable(test ${CMAKE_CURRENT_LIST_DIR}/test/main.cpp)
    target_link_libraries(test ${PROJECT_NAME})
    target_include_directories(test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
endif()

install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
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


