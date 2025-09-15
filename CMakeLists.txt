cmake_minimum_required(VERSION 3.10)
project(vp VERSION 1.8 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include GNUInstallDirs for standard installation directories
include(GNUInstallDirs)

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# Add custom module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find required packages
find_package(PkgConfig)

# Find SDL2 - try pkg-config first, then fallback to find modules
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 sdl2>=2.0.0)
    pkg_check_modules(SDL2_IMAGE SDL2_image>=2.0.0)
endif()

# Fallback to find modules if pkg-config failed
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()

if(NOT SDL2_IMAGE_FOUND)
    find_package(SDL2_image REQUIRED)
endif()

# Find X11 for X11 support
find_package(X11)

# Find math library
find_library(MATH_LIBRARY m)

# Check for required headers
include(CheckIncludeFile)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(time.h HAVE_TIME_H)

# Check for required functions
include(CheckFunctionExists)
check_function_exists(mkstemps HAVE_MKSTEMPS)
check_function_exists(getopt_long HAVE_GETOPT_LONG)

# Generate config header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(doc)

# Install documentation files if they exist
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/README)
    install(FILES README
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT documentation)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
    install(FILES COPYING
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT documentation)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ChangeLog)
    install(FILES ChangeLog
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
        COMPONENT documentation)
endif()

# CPack configuration for packaging
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SDL-based image viewer for Linux and FreeBSD")
set(CPACK_PACKAGE_VENDOR "Erik Greenwald")
set(CPACK_PACKAGE_CONTACT "erik@elfga.com")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")

# RPM specific
set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")
set(CPACK_RPM_PACKAGE_LICENSE "GPL")

# DEB specific
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, libsdl2-image-2.0-0")

include(CPack)