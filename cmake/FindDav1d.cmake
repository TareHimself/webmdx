# Finddav1d.cmake
find_path(dav1d_INCLUDE_DIR
        NAMES dav1d.h
        PATH_SUFFIXES dav1d
)

find_library(dav1d_LIBRARY
        NAMES "dav1d"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(dav1d
        REQUIRED_VARS
        dav1d_INCLUDE_DIR
        dav1d_LIBRARY
)

if(dav1d_FOUND)
    set(dav1d_INCLUDE_DIRS ${dav1d_INCLUDE_DIR})
    set(dav1d_LIBRARIES ${dav1d_LIBRARY})
endif()

mark_as_advanced(
        dav1d_INCLUDE_DIR
        dav1d_LIBRARY
)