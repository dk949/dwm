include(${CMAKE_SOURCE_DIR}/cmake/misc.cmake)
function (enable_sanitizers project_name #[[access]])
    optional_args(acc DEFAULTS "PRIVATE" ARGS ${ARGN})

    if (NOT ENABLE_SANITIZERS)
        return()
    endif ()

    if (MSVC)
        return()
    endif ()

    message(STATUS "Running with sanitizers: [${SANITIZER_LIST}]")
    target_compile_options(${project_name} ${acc_0} -fsanitize=${SANITIZER_LIST})
    target_link_options(${project_name} ${acc_0} -fsanitize=${SANITIZER_LIST})
endfunction ()
