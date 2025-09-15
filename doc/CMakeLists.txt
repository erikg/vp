# Documentation files

# Optionally compress man page if gzip is available
find_program(GZIP_EXECUTABLE gzip)
if(GZIP_EXECUTABLE)
    # Create compressed man page during build
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vp.1.gz
        COMMAND ${GZIP_EXECUTABLE} -c ${CMAKE_CURRENT_SOURCE_DIR}/vp.1 > ${CMAKE_CURRENT_BINARY_DIR}/vp.1.gz
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/vp.1
        COMMENT "Compressing man page"
    )

    add_custom_target(man_page ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/vp.1.gz)

    # Install compressed version
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/vp.1.gz
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
        COMPONENT documentation
    )
else()
    # Install uncompressed version
    install(FILES vp.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
        COMPONENT documentation
    )
endif()