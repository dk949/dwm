# Use the toolchain file
set(CMAKE_TOOLCHAIN_FILE
    "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING ""
)

# Read manifest
file(READ "${CMAKE_SOURCE_DIR}/vcpkg.json" _manifest)

# Get project name from manifest
# cmake-format: off
string(
    JSON THIS_PROJECT_NAME
    ERROR_VARIABLE _err
    GET ${_manifest} "name"
)
# cmake-format: on
if (_err)
    message(FATAL_ERROR "Could not get project name from manifest: ${_err}")
endif ()

string(REPLACE "-" "_" THIS_PROJECT_NAME ${THIS_PROJECT_NAME})

# get versino from manifest. try all formats until one works
set(_version_types "version-string" "version" "version-semver" "version-date")
foreach (_version ${_version_types})

    # cmake-format: off
    string(
        JSON THIS_PROJECT_VERSION
        ERROR_VARIABLE _err
        GET ${_manifest} "${_version}"
    )# cmake-format: on

    if (_err STREQUAL "NOTFOUND")
        break()
    endif ()
endforeach ()
if (_err)
    message(FATAL_ERROR "Could not get project version from manifest: ${_err}")
endif ()

# get description from manifest.
# cmake-format: off
string(
    JSON _desc
    ERROR_VARIABLE _err
    GET ${_manifest} "description"
)
# cmake-format: on
if (_err)
    message(FATAL_ERROR "Could not get project description from manifest: ${_err}")
endif ()

if (NOT ${_desc} STREQUAL "")
    set(THIS_PROJECT_DESCRIPTION DESCRIPTION ${_desc})
endif ()

# get url from manifest.
# cmake-format: off
string(
    JSON _url
    ERROR_VARIABLE _err
    GET ${_manifest} "$url"
)
# cmake-format: on
if (_err)
    message(FATAL_ERROR "Could not get project url from manifest: ${_err}")
endif ()

if (NOT ${_url} STREQUAL "")
    set(THIS_PROJECT_HOMEPAGE_URL HOMEPAGE_URL ${_url})
endif ()

unset(_err)
unset(_version_types)
unset(_desc)
unset(_url)
