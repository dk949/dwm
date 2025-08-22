# cmake-format: off
include(${CMAKE_SOURCE_DIR}/cmake/options_support.cmake)


# Targets
set(EXE_NAME ${CMAKE_PROJECT_NAME} CACHE STRING "Name of the main executable")
set(DOXYFILE_SUFFIX ${CMAKE_PROJECT_NAME}_docs CACHE STRING "Documentation target")

# Artifacts
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE STRING "archive location")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE STRING "library location")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE STRING "executable location")

# Analysers
option(ENABLE_CPPCHECK "Enable cppcheck" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(ENABLE_CLANG_TIDY_FULL "Enable more clang-tidy checks (takes more time)" OFF)
option(ENABLE_INCLUDE_WHAT_YOU_USE "Enable include-what-you-use" OFF)



# Sanitizers
option(ENABLE_SANITIZERS "Enable sanitizers" ${IS_DEBUG})
set(SANITIZER_LIST "address,leak,undefined" CACHE STRING "List of sanitizers to use")



# Docs
option(ENABLE_DOXYGEN "Enable doxygen doc builds of source" ON)



# Warnings
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)



# Caching
option(ENABLE_CACHE "Enable cache if available" ON)
set(CACHE_PROGRAM "ccache" CACHE STRING "Compiler cache to be used")

# Macro prefix

set(MACRO_PREFIX "" CACHE STRING "Make __FILE__ macro relative to particular directory")

# Language standard and extensions
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

# event logging
option(LOG_EVENTS "log event handling stats" OFF)

# Compile commands
option(CMAKE_EXPORT_COMPILE_COMMANDS "generate compile_commands.json" ON)

set(FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/_deps" CACHE STRING "base directory to fetch content into")

# cmake-format: on
