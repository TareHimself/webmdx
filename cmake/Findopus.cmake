# Findopus.cmake
find_path(opus_INCLUDE_DIR
    NAMES opus.h
        PATH_SUFFIXES opus
)

find_library(opus_LIBRARY
    NAMES "opus"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(opus
    REQUIRED_VARS
        opus_INCLUDE_DIR
        opus_LIBRARY
)

if(opus_FOUND)
    set(opus_INCLUDE_DIRS ${opus_INCLUDE_DIR})
    set(opus_LIBRARIES ${opus_LIBRARY})
endif()

mark_as_advanced(
        opus_INCLUDE_DIR
        opus_LIBRARY
)
