# Source files
set(VP_SOURCES
    vp.c
    image.c
    input.c
    net.c
    http.c
    ftp.c
    timer.c
)

# Headers
set(VP_HEADERS
    vp.h
    image.h
    input.h
    net.h
    http.h
    ftp.h
    timer.h
)

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

# Link libraries
target_link_libraries(vp
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
)

# Link math library if found
if(MATH_LIBRARY)
    target_link_libraries(vp ${MATH_LIBRARY})
endif()

# Link X11 if found
if(X11_FOUND)
    target_link_libraries(vp ${X11_LIBRARIES})
    target_include_directories(vp PRIVATE ${X11_INCLUDE_DIR})
endif()

# Install executable
install(TARGETS vp DESTINATION bin)

# Install headers (optional - for development)
if(INSTALL_HEADERS)
    install(FILES ${VP_HEADERS} DESTINATION include/vp)
endif()