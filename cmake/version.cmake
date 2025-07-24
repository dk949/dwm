set(VERSION
    ""
    CACHE STRING "Project version"
)

if (VERSION STREQUAL "")
    execute_process(
        COMMAND git log -1 --format=%cd --date=format:%F
        OUTPUT_VARIABLE date
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(date_time "${date} 00:00")

    execute_process(
        COMMAND git rev-list --count HEAD --since="${date_time}"
        OUTPUT_VARIABLE commit_count
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND date -d "${date}" +%Y%m%d
        OUTPUT_VARIABLE patch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(VERSION
        "${THIS_PROJECT_VERSION}.${patch}_${commit_count}"
        CACHE STRING "Project version" FORCE
    )
endif ()
