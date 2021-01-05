########################## Version and language setup ##########################

# Minimum version of cmake required to run this project
cmake_minimum_required(VERSION 3.16)

# Name of the project, languages used, version of the project
project(dwm LANGUAGES C VERSION 6.2)
add_compile_definitions(VERSION="${PROJECT_VERSION}")

# Language standards
set(CMAKE_C_STANDARD 11)

################################## File setup ##################################
# Defining project files
set(SOURCE_FILES
        xbacklight.c
        xbacklight.h
        main.c)
set(HEADER_FILES )

############################### Executable setup ###############################
# Adding the executable
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

######################## Compiler and linker flag setup ########################

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_C_FLAGS "")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lxcb-randr -lxcb -lxcb-util ")

############################## installation setup ##############################
SET(CMAKE_INSTALL_PREFIX "/usr/local")
INSTALL(TARGETS ${PROJECT_NAME} RUNTIME)
INSTALL(FILES a.1 TYPE MAN)

# CMake doesn't have a native uninstall target
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

################################## Docs setup ##################################




option(BUILD_DOC "Build documentation" ON)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # bright_set_ input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target( docs ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating documentation with Doxygen"
            VERBATIM )
else (DOXYGEN_FOUND)
    message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)
