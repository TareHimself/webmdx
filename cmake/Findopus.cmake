# FindWebm.cmake
message(FATAL_ERROR "Findopus Not Implemented")
find_path(webm_INCLUDE_DIR
    NAMES mkvmuxer/mkvmuxer.h
    PATH_SUFFIXES webm
)

find_library(webm_LIBRARY
    NAMES "webm"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(webm
    REQUIRED_VARS
        webm_INCLUDE_DIR
        webm_LIBRARY
)

if(webm_FOUND)
    set(webm_INCLUDE_DIRS ${webm_INCLUDE_DIR})
    set(webm_LIBRARIES ${webm_LIBRARY})
endif()

## Optional: wayland-cursor (used for pointer images)
#find_path(WAYLAND_CURSOR_INCLUDE_DIR
#    NAMES wayland-cursor.h
#    PATH_SUFFIXES wayland
#)
#
#find_library(WAYLAND_CURSOR_LIBRARY
#    NAMES wayland-cursor
#)

#if(WAYLAND_CURSOR_LIBRARY AND WAYLAND_CURSOR_INCLUDE_DIR)
#    set(WAYLAND_CURSOR_LIBRARIES ${WAYLAND_CURSOR_LIBRARY})
#    set(WAYLAND_CURSOR_INCLUDE_DIRS ${WAYLAND_CURSOR_INCLUDE_DIR})
#endif()

mark_as_advanced(
        webm_INCLUDE_DIR
        webm_LIBRARY
)
