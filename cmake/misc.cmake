#[[
There isn't a way (to my knowledge) to `unset` a function name in cmake,
this makes calling an unwanted function (or macro) a fatal error.

typical usage:

```cmake
# file1.cmake

function(do_thing)
    ...
endfunction()

do_thing()
UNSET_FUNCTION(do_thing)
```

```cmake
# file2.cmake

function(do_thing)
    ...
endfunction()

do_thing() # OK because function is redefined
UNSET_FUNCTION(do_thing)
```

```cmake
# file3.cmake

do_thing() # ERROR
```

]]
function (unset_function func)
    function (${func})
        message(FATAL_ERROR "Calling a deleted function \"${CMAKE_CURRENT_FUNCTION}\"")
    endfunction ()
endfunction ()

#[[
optional_args(<prefix> DEFAULTS <defaults>... ARGS <args>...)

<prefix_n> corresponds to the nth argument or the nth default if the argument is missing
<prefix>_N denotes the total number of arguments

NOTE: <prefix>_N - 1 is the last <prefix_n>


typical usage:

```cmake
function(foo required_arg)
    optional_args(optional_arg DEFAULTS "world" ARGS ${ARNG})
    message("${required_arg} ${optional_arg_0}")
endfunction()


foo("hello")
# output: hello world
foo("hello" "bob")
# output: hello bob
```

]]
macro (optional_args out_prefix)
    cmake_parse_arguments(args "" "" "ARGS;DEFAULTS" ${ARGN})
    set(i 0)
    foreach (arg def IN ZIP_LISTS args_ARGS args_DEFAULTS)
        if (DEFINED arg)
            set(${out_prefix}_${i} ${arg})
        else ()
            set(${out_prefix}_${i} ${def})
        endif ()
        math(EXPR i "${i} + 1")
    endforeach ()
    set(${out_prefix}_N ${i})
endmacro ()

#[[
adds a dummy c++ file and sets `source_files` to the path to this file
]]
macro (add_dummy_source source_files)
    if (NOT ${source_files})
        set(fname ${CMAKE_BINARY_DIR}/dummy.cpp)
        file(WRITE ${fname} "[[maybe_unused]] static int i;")
        set(${source_files} ${fname})
    endif ()
endmacro ()
