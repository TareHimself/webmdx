# FindYuv.cmake
find_path(Yuv_INCLUDE_DIR
    NAMES planar_functions.h
        PATH_SUFFIXES libyuv
)

find_library(Yuv_LIBRARY
    NAMES yuv yuv
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Yuv
    REQUIRED_VARS
        Yuv_INCLUDE_DIR
        Yuv_LIBRARY
)

if(Yuv_FOUND)
    set(Yuv_INCLUDE_DIRS ${Yuv_INCLUDE_DIR})
    set(Yuv_LIBRARIES ${Yuv_LIBRARY})
endif()

mark_as_advanced(
    Yuv_INCLUDE_DIR
    Yuv_LIBRARY
)
