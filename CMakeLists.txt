cmake_minimum_required(VERSION 3.25)
include(cmake/setup_pkg.cmake)
include(cmake/version.cmake)
project(
    ${THIS_PROJECT_NAME}
    VERSION ${THIS_PROJECT_VERSION} ${THIS_PROJECT_DESCRIPTION} ${THIS_PROJECT_HOMEPAGE_URL}
    LANGUAGES CXX C
)

include(cmake/options.cmake)

include(cmake/static_analyzers.cmake)
include(cmake/cache.cmake)
include(cmake/compile_commands.cmake)
include(cmake/compiler_warnings.cmake)
include(cmake/doxygen.cmake)
include(cmake/sanitizers.cmake)
include(cmake/macro_prefix.cmake)
include(cmake/x11.cmake)
include(cmake/alsa.cmake)
include(cmake/icons.cmake)

add_subdirectory(src)
add_subdirectory(project_config)
install(TARGETS ${EXE_NAME})
