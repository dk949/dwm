include(${CMAKE_SOURCE_DIR}/cmake/misc.cmake)

function (set_macro_prefix project_name acc)
    if (NOT ENABLE_MACRO_PREFIX)
        return()
    endif ()
    if (ENABLE_COVERAGE)
        return()
    endif ()
    if (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(
            ${project_name} ${acc} #
            "-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=${MACRO_PREFIX}"
            "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=${MACRO_PREFIX}"
        )
    endif ()
endfunction ()
