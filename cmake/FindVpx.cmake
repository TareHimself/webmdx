# FindVpx.cmake
find_path(Vpx_INCLUDE_DIR
    NAMES vpx_decoder.h
        PATH_SUFFIXES vpx
)

find_library(Vpx_LIBRARY
    NAMES "vpx"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vpx
    REQUIRED_VARS
        Vpx_INCLUDE_DIR
        Vpx_LIBRARY
)

if(Vpx_FOUND)
    set(Vpx_INCLUDE_DIRS ${Vpx_INCLUDE_DIR})
    set(Vpx_LIBRARIES ${Vpx_LIBRARY})
endif()

mark_as_advanced(
        Vpx_INCLUDE_DIR
        Vpx_LIBRARY
)
