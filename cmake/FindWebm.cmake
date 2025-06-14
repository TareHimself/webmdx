# FindWebm.cmake
find_path(Webm_INCLUDE_DIR
    NAMES mkvmuxer/mkvmuxer.h
    PATH_SUFFIXES webm
)

find_library(Webm_LIBRARY
    NAMES "webm"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Webm
    REQUIRED_VARS
        Webm_INCLUDE_DIR
        Webm_LIBRARY
)

if(Webm_FOUND)
    set(Webm_INCLUDE_DIRS ${Webm_INCLUDE_DIR})
    set(Webm_LIBRARIES ${Webm_LIBRARY})
endif()

mark_as_advanced(
        Webm_INCLUDE_DIR
        Webm_LIBRARY
)
