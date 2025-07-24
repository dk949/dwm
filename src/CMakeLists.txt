include(FetchContent)
include(CMakePrintHelpers)
link_compile_commands(compile_commands.json)

FetchContent_Declare(
    libut
    GIT_REPOSITORY https://github.com/dk949/libut/
    GIT_TAG trunk
)
FetchContent_Declare(
    noticeboard
    GIT_REPOSITORY https://github.com/dk949/noticeboard/
    GIT_TAG trunk
)

FetchContent_MakeAvailable(libut)
FetchContent_MakeAvailable(noticeboard)


add_executable(
    ${EXE_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/backlight.cpp ${CMAKE_CURRENT_SOURCE_DIR}/drw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dwm.cpp ${CMAKE_CURRENT_SOURCE_DIR}/log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util.cpp ${CMAKE_CURRENT_SOURCE_DIR}/volc.cpp
)

target_link_libraries(${EXE_NAME} PUBLIC UT::all)
target_link_libraries(${EXE_NAME} PUBLIC noticeboard)
execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE CMAKE_PROPERTY_LIST)
# Convert command output into a CMake list
string(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
string(REGEX REPLACE "\n" ";" CMAKE_PROPERTY_LIST "${CMAKE_PROPERTY_LIST}")
list(REMOVE_DUPLICATES CMAKE_PROPERTY_LIST)
# cmake_print_properties(TARGETS noticeboard PROPERTIES "${CMAKE_PROPERTY_LIST}")

target_sources(
    ${EXE_NAME}
    PUBLIC FILE_SET #
           HEADERS BASE_DIRS #
           ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/project_config/include
)

target_add_icon_loc(${EXE_NAME} PUBLIC)
target_link_alsa(${EXE_NAME} PUBLIC)
target_link_xinerama(${EXE_NAME} PUBLIC)
target_link_x11(${EXE_NAME} PUBLIC Xft xcb_res xcb X11_xcb)


enable_sanitizers(${EXE_NAME} PUBLIC)
set_target_warnings(${EXE_NAME} PUBLIC)
set_macro_prefix(${EXE_NAME} PUBLIC)
