# Findvpx.cmake
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

mark_as_advanced(
        vpx_INCLUDE_DIR
        vpx_LIBRARY
)