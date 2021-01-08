########################## Version and language setup ##########################

# Minimum version of cmake required to run this project
cmake_minimum_required(VERSION 3.16)

# Name of the project, languages used, version of the project
project(dwm LANGUAGES C VERSION 6.2)

# Language standards
set(CMAKE_C_STANDARD 99)

################################## File setup ##################################
# Defining project files
set(SOURCE_FILES
        drw.c
        dwm.c
        util.c
        xbacklight.c
)

set(HEADER_FILES
        config.h
        drw.h
        util.h
        xbacklight.h
    )

############################### Executable setup ###############################
# Adding the executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

################################## Libs setup ##################################
set(X11_DEPS "X11" "X11-xcb" "Xft" "Xinerama")
set(XCB_DEPS "xcb" "xcb-randr" "xcb-res" "xcb-util")
set(FREETYPE_DEPS "fontconfig")


include_directories (/usr/include/freetype2)

foreach(LIB IN LISTS X11_DEPS XCB_DEPS FREETYPE_DEPS)
    find_library(${LIB}_LIB ${LIB})

    # Xinerama is an optional library, if tehre are more optional libraries,
    # they will be brought out into a seporate loop
    if("${LIB}" STREQUAL "Xinerama")
        if(NOT ${LIB}_LIB)
            message(WARNING "l${LIB} was not found. Proceeding without it")
        else()
            target_link_libraries(${PROJECT_NAME} PRIVATE ${${LIB}_LIB})
            add_compile_definitions(XINERAMA=1)
        endif()
    else()
        if (NOT ${LIB}_LIB)
            message(FATAL_ERROR "l${LIB} was not found. Cannot proceed.")
        else()
            target_link_libraries(${PROJECT_NAME} PRIVATE ${${LIB}_LIB})
        endif()
    endif()
endforeach()

################################ Compiler setup ################################
# definitions
add_compile_definitions(
    VERSION="${PROJECT_VERSION}"
    _DEFAULT_SOURCE
    _BSD_SOURCE
    _POSIX_C_SOURCE=200809L
    )

# warnings
add_compile_options("-pedantic" "-Wall" "-Werror")

# optmisation
add_compile_options(-Os)

############################## installation setup ##############################
SET(CMAKE_INSTALL_PREFIX "/usr/local")
INSTALL(TARGETS ${PROJECT_NAME} RUNTIME)
INSTALL(FILES ${PROJECT_NAME}.1 TYPE MAN)

# uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/modules/uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/modules/uninstall.cmake)
endif()
