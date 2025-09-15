# FindSDL2_image.cmake - Locate SDL2_image library
# This module defines:
#  SDL2_IMAGE_LIBRARIES, the name of the library to link against
#  SDL2_IMAGE_INCLUDE_DIRS, where to find the headers
#  SDL2_IMAGE_FOUND, if false, do not try to link against
#  SDL2_IMAGE_VERSION_STRING - human-readable string containing the version

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(_SDL2_IMAGE SDL2_image)
endif()

find_path(SDL2_IMAGE_INCLUDE_DIR
    NAMES SDL_image.h
    HINTS
        ${_SDL2_IMAGE_INCLUDEDIR}
        ${_SDL2_IMAGE_INCLUDE_DIRS}
    PATH_SUFFIXES SDL2
)

find_library(SDL2_IMAGE_LIBRARY
    NAMES SDL2_image
    HINTS
        ${_SDL2_IMAGE_LIBDIR}
        ${_SDL2_IMAGE_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_image
    REQUIRED_VARS SDL2_IMAGE_LIBRARY SDL2_IMAGE_INCLUDE_DIR
)

if(SDL2_IMAGE_FOUND)
    set(SDL2_IMAGE_LIBRARIES ${SDL2_IMAGE_LIBRARY})
    set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})
endif()

mark_as_advanced(SDL2_IMAGE_INCLUDE_DIR SDL2_IMAGE_LIBRARY)