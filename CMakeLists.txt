cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

project(webmdx LANGUAGES C CXX VERSION 1.0.0 DESCRIPTION "C++ library for decoding of webm files")

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/lib/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/lib/*.h")

add_library(${PROJECT_NAME} ${SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

#find_package(PkgConfig REQUIRED)
find_package(webm REQUIRED)
find_package(libyuv REQUIRED)


target_link_libraries(${PROJECT_NAME} PRIVATE webm::webm libyuv::libyuv)
message(STATUS "FLAGS ")
#pkg_check_modules(VPX vpx)
find_package(libvpx)
if(libvpx_FOUND)
    message(STATUS "Vpx Enabled")
    target_link_libraries(${PROJECT_NAME} PRIVATE libvpx::libvpx)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_CODEC_VIDEO_VPX)
else()
    message(STATUS "Vpx Disabled")
endif()

find_package(dav1d)
if(dav1d_FOUND)
    message(STATUS "Av1 Enabled")
    target_link_libraries(${PROJECT_NAME} PRIVATE dav1d::dav1d)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_CODEC_VIDEO_DAV1D)
else()
    message(STATUS "Av1 Disabled")
endif()

find_package(Opus CONFIG)
if(Opus_FOUND)
    message(STATUS "Opus Enabled")
    target_link_libraries(${PROJECT_NAME} PRIVATE Opus::opus)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_CODEC_AUDIO_OPUS)
else()
    message(STATUS "Opus Disabled")
endif()

find_package(Vorbis CONFIG)
if(Vorbis_FOUND)
    message(STATUS "Vorbis Enabled")
    target_link_libraries(${PROJECT_NAME} PRIVATE Vorbis::vorbis)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_CODEC_AUDIO_VORBIS)
else()
    message(STATUS "Vorbis Disabled")
endif()

get_target_property(WEBMDX_LIBRARY_TYPE ${PROJECT_NAME} TYPE)
if(WEBMDX_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(${PROJECT_NAME} PRIVATE WEBM_DX_PRODUCER)
else()
    target_compile_definitions(${PROJECT_NAME} PUBLIC WEBMDX_STATIC)
endif()

include(GNUInstallDirs)


# Install headers
install(
    DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the shared/static library
install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}    # .so/.dylib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}    # .lib (static or import)
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}    # .dll
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the CMake config package files
install(
    EXPORT ${PROJECT_NAME}-targets
    FILE ${PROJECT_NAME}Config.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)


