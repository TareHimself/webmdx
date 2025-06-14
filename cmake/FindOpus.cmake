# FindOpus.cmake
find_path(Opus_INCLUDE_DIR
    NAMES opus.h
        PATH_SUFFIXES opus
)

find_library(Opus_LIBRARY
    NAMES "opus"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus
    REQUIRED_VARS
        Opus_INCLUDE_DIR
        Opus_LIBRARY
)

if(Opus_FOUND)
    set(Opus_INCLUDE_DIRS ${Opus_INCLUDE_DIR})
    set(Opus_LIBRARIES ${Opus_LIBRARY})
endif()

mark_as_advanced(
        Opus_INCLUDE_DIR
        Opus_LIBRARY
)
