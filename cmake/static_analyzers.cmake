include(${CMAKE_SOURCE_DIR}/cmake/misc.cmake)

macro (_enable_stat _stat)
    string(TOUPPER ${_stat} _STAT)
    string(REPLACE "-" "_" _STAT ${_STAT})
    if (ENABLE_${_STAT})
        find_program(_${_STAT} ${_stat})
        if (_${_STAT})
            set(CMAKE_CXX_${_STAT} ${_${_STAT}} ${ARGN})
            message(STATUS "${_stat} found and enabled")
        else ()
            message(WARNING "${_stat} requested but executable not found")
        endif ()
    endif ()
endmacro ()

_enable_stat(cppcheck --suppress=missingInclude --enable=all --inline-suppr --inconclusive)

if (ENABLE_CLANG_TIDY_FULL)
    if (NOT ENABLE_CLANG_TIDY)
        message(AUTHOR_WARNING "Using ENABLE_CLANG_TIDY_FULL without ENABLE_CLANG_TIDY, is meaningless")
    else ()
        set(full_check_list
            [[
bugprone-infinite-loop,
bugprone-reserved-identifier,
bugprone-stringview-nullptr,
bugprone-suspicious-string-compare,
bugprone-use-after-move,
misc-confusable-identifiers,
misc-const-correctness,
misc-definitions-in-headers,
misc-unused-alias-decls,
misc-unused-using-decls,
modernize-macro-to-enum,
readability-container-size-empty,
readability-identifier-naming,
cppcoreguidelines-owning-memory,
readability-uppercase-literal-suffix,
readability-non-const-parameter,
]]
        )
        string(REPLACE "\n" "" full_check_list ${full_check_list})

        message(STATUS "enabled additional clang tidy checks")
    endif ()
endif ()
_enable_stat(clang-tidy --extra-arg=-Wno-unknown-warning-option --checks="${full_check_list}")
unset(full_check_list)

_enable_stat(include-what-you-use)

unset_function(_enable_stat)
