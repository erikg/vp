# Source files
set(VP_SOURCES
    vp.c
    image.c
    input.c
    net.c
    http.c
    timer.c
)

# Headers
set(VP_HEADERS
    vp.h
    image.h
    input.h
    net.h
    http.h
    timer.h
    font8x8.h
)

# pkg-config reports bare library names in *_LIBRARIES; the matching -L
# paths are in *_LIBRARY_DIRS and must be passed too, or the link fails
# anywhere SDL2 is off the default linker path (/usr/local/lib on FreeBSD).
link_directories(${SDL2_LIBRARY_DIRS} ${SDL2_IMAGE_LIBRARY_DIRS})

# Create executable
add_executable(vp ${VP_SOURCES})

# Include directories
target_include_directories(vp PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/../  # For config.h
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(vp PRIVATE HAVE_CONFIG_H)

# Compiler flags
target_compile_options(vp PRIVATE
    ${SDL2_CFLAGS_OTHER}
    ${SDL2_IMAGE_CFLAGS_OTHER}
)

# Link libraries. SDL2_image.pc "Requires: sdl2", so SDL2_IMAGE_LIBRARIES
# already repeats SDL2's libs; dedupe the combined list or CMake emits them
# twice and the macOS linker warns about duplicate libraries. SDL2_image
# goes first so static-link ordering stays correct after dedup.
set(VP_LINK_LIBRARIES ${SDL2_IMAGE_LIBRARIES} ${SDL2_LIBRARIES})

if(MATH_LIBRARY)
    list(APPEND VP_LINK_LIBRARIES ${MATH_LIBRARY})
endif()

if(X11_FOUND)
    list(APPEND VP_LINK_LIBRARIES ${X11_LIBRARIES})
    target_include_directories(vp PRIVATE ${X11_INCLUDE_DIR})
endif()

if(OPENSSL_FOUND)
    list(APPEND VP_LINK_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
endif()

list(REMOVE_DUPLICATES VP_LINK_LIBRARIES)
target_link_libraries(vp ${VP_LINK_LIBRARIES})

# Install executable
install(TARGETS vp DESTINATION bin)

# Install headers (optional - for development)
if(INSTALL_HEADERS)
    install(FILES ${VP_HEADERS} DESTINATION include/vp)
endif()