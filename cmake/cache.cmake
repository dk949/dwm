include(${CMAKE_SOURCE_DIR}/cmake/misc.cmake)

function (_enable_cache)
    if (NOT ENABLE_CACHE)
        return()
    endif ()

    find_program(cache_bin ${CACHE_PROGRAM})
    if (cache_bin)
        message(STATUS "${CACHE_PROGRAM} found and enabled")
        set(CMAKE_CXX_COMPILER_LAUNCHER
            ${cache_bin}
            CACHE STRING "cmake compiler launcher"
        )
    else ()
        message(WARNING "${CACHE_PROGRAM} is enabled but was not found. Not using it")
    endif ()
endfunction ()

_enable_cache()
unset_function(_enable_cache)
