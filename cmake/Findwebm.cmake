# Findwebm.cmake

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

mark_as_advanced(
        webm_INCLUDE_DIR
        webm_LIBRARY
)
