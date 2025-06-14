# FindDav1d.cmake
find_path(Dav1d_INCLUDE_DIR
    NAMES dav1d.h
        PATH_SUFFIXES dav1d
)

find_library(Dav1d_LIBRARY
    NAMES "dav1d"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Dav1d
    REQUIRED_VARS
        Dav1d_INCLUDE_DIR
        Dav1d_LIBRARY
)

if(Dav1d_FOUND)
    set(Dav1d_INCLUDE_DIRS ${Dav1d_INCLUDE_DIR})
    set(Dav1d_LIBRARIES ${Dav1d_LIBRARY})
endif()

mark_as_advanced(
        Dav1d_INCLUDE_DIR
        Dav1d_LIBRARY
)
