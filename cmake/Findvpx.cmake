# Findvps.cmake
find_path(vpx_INCLUDE_DIR
    NAMES vpx_decoder.h
        PATH_SUFFIXES vpx
)

find_library(vpx_LIBRARY
    NAMES "vpx"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(vpx
    REQUIRED_VARS
        vpx_INCLUDE_DIR
        vpx_LIBRARY
)

if(vpx_FOUND)
    set(vpx_INCLUDE_DIRS ${vpx_INCLUDE_DIR})
    set(vpx_LIBRARIES ${vpx_LIBRARY})
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
        vpx_INCLUDE_DIR
        vpx_LIBRARY
)
