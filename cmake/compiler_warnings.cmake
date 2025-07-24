# from here:
#
# https://github.com/lefticus/cppbestpractices/blob/master/02-Use_the_Tools_Available.md

include(${CMAKE_SOURCE_DIR}/cmake/misc.cmake)

function (set_target_warnings project_name #[[access]])
    macro (gnu_add_no_error warn_list warn)
        list(APPEND ${warn_list} "-W${warn}")
        list(APPEND ${warn_list} "-Wno-error=${warn}")
    endmacro ()

    optional_args(acc DEFAULTS "PRIVATE" ARGS ${ARGN})
    set(MSVC_WARNINGS
        /W4 # Baseline reasonable warnings
        /w14242 # 'identifier': conversion from 'type1' to 'type1', possible loss of data
        /w14254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
        /w14263 # 'function': member function does not override any base class virtual member function
        /w14265 # 'classname': class has virtual functions, but destructor is not virtual instances of this
                # class may not be destructed correctly
        /w14287 # 'operator': unsigned/negative constant mismatch
        /we4289 # nonstandard extension used: 'variable': loop control variable declared in the for-loop is
                # used outside the for-loop scope
        /w14296 # 'operator': expression is always 'boolean_value'
        /w14311 # 'variable': pointer truncation from 'type1' to 'type2'
        /w14545 # expression before comma evaluates to a function which is missing an argument list
        /w14546 # function call before comma missing argument list
        /w14547 # 'operator': operator before comma has no effect; expected operator with side-effect
        /w14549 # 'operator': operator before comma has no effect; did you intend 'operator'?
        /w14555 # expression has no effect; expected expression with side- effect
        /w14619 # pragma warning: there is no warning number 'number'
        /w14640 # Enable warning on thread un-safe static member initialization
        /w14826 # Conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime
                # behavior.
        /w14905 # wide string literal cast to 'LPSTR'
        /w14906 # string literal cast to 'LPWSTR'
        /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly
                # applied
        /permissive- # standards conformance mode for MSVC compiler.
    )

    set(COMMON_WARNINGS
        -Wall
        -Wextra # reasonable and standard
        -Wshadow # warn the user if a variable declaration shadows one from a parent context
        -Wnon-virtual-dtor # warn the user if a class with virtual functions has a non-virtual destructor.
                           # This helps catch hard to track down memory errors
        # -Wold-style-cast # warn for c-style casts
        -Wcast-align # warn for potential performance problem casts
        -Wunused # warn on anything being unused
        -Woverloaded-virtual # warn if you overload (not override) a virtual function
        -Wpedantic # warn if non-standard C++ is used
        -Wconversion # warn on type conversions that may lose data
        -Wsign-conversion # warn on sign conversions
        -Wnull-dereference # warn if a null dereference is detected
        -Wdouble-promotion # warn if float is implicit promoted to double
        -Wformat=2 # warn on security issues around functions that format output (ie printf)
        -Wswitch-default # warn if switch is missing default
        -Wswitch-enum # warn if not all enum members are covered by the switch, even with default specified
        -Wctad-maybe-unsupported # CTAD guides were not provided (CTAD may break with other compilers)
        -Wimplicit-fallthrough # implicit fallthrough for cases
        -Wmisleading-indentation # warn if indentation implies blocks where blocks do not exist
        -Wsuggest-override # suggest virtual function is marked override if it overrides something
    )
    # Warnings which are not errors even when -Werror is on
    gnu_add_no_error(COMMON_WARNINGS unused-but-set-parameter)
    gnu_add_no_error(COMMON_WARNINGS unused-but-set-variable)
    gnu_add_no_error(COMMON_WARNINGS unused-const-variable)
    gnu_add_no_error(COMMON_WARNINGS unused-function)
    gnu_add_no_error(COMMON_WARNINGS unused-label)
    gnu_add_no_error(COMMON_WARNINGS unused-local-typedefs)
    gnu_add_no_error(COMMON_WARNINGS unused-macros)
    gnu_add_no_error(COMMON_WARNINGS unused-parameter)
    gnu_add_no_error(COMMON_WARNINGS unused-variable)

    if (WARNINGS_AS_ERRORS)
        set(COMMON_WARNINGS ${COMMON_WARNINGS} -Werror)
        set(MSVC_WARNINGS ${MSVC_WARNINGS} /WX)
    endif ()

    set(GCC_WARNINGS
        ${COMMON_WARNINGS}
        -Wduplicated-cond # warn if if / else chain has duplicated conditions
        -Wduplicated-branches # warn if if / else branches have duplicated code
        -Wlogical-op # warn about logical operations being used where bitwise were probably wanted
        -Wuseless-cast # warn if you perform a cast to the same type
        -Wnoexcept # noextept(func()) is false, because func is not noexcept, but it can be
        -Wunused-const-variable=1 # using level 1, because default (set above) also accounts for headers
    )

    set(CLANG_WARNINGS #
        ${COMMON_WARNINGS} -Wused-but-marked-unused #< this is only for [[gnu::unused]], not [[maybe_unused]]
    )
    gnu_add_no_error(CLANG_WARNINGS unused-private-field)
    gnu_add_no_error(CLANG_WARNINGS unused-comparison)
    gnu_add_no_error(CLANG_WARNINGS unused-exception-parameter)
    gnu_add_no_error(CLANG_WARNINGS unused-lambda-capture)
    gnu_add_no_error(CLANG_WARNINGS unused-member-function)
    gnu_add_no_error(CLANG_WARNINGS unused-template)

    if (MSVC)
        target_compile_options(${project_name} ${acc_0} ${MSVC_WARNINGS})
    elseif (CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        target_compile_options(${project_name} ${acc_0} ${CLANG_WARNINGS})
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${project_name} ${acc_0} ${GCC_WARNINGS})
    else ()
        message(AUTHOR_WARNING "No compiler warnings set for '${CMAKE_CXX_COMPILER_ID}' compiler.")
    endif ()

endfunction ()
