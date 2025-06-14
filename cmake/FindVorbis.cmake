# FindVorbis.cmake
find_path(Vorbis_INCLUDE_DIR
    NAMES codec.h
        PATH_SUFFIXES vorbis
)

find_library(Vorbis_LIBRARY
    NAMES "vorbis"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vorbis
    REQUIRED_VARS
        Vorbis_INCLUDE_DIR
        Vorbis_LIBRARY
)

if(Vorbis_FOUND)
    set(Vorbis_INCLUDE_DIRS ${Vorbis_INCLUDE_DIR})
    set(Vorbis_LIBRARIES ${Vorbis_LIBRARY})
endif()

mark_as_advanced(
        Vorbis_INCLUDE_DIR
        Vorbis_LIBRARY
)
