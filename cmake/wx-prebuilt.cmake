# Static wxWidgets from the native-deps Forgejo releases.
#
# Downloads (once per build dir) the pinned tarball, verifies its SHA-256,
# extracts it, and exposes:
#   WX_PREBUILT_PREFIX - the install prefix (contains bin/wx-config)
#   OWON_WX_CONFIG     - wrapper that forces wx-config onto that prefix
#
# Pins are CACHE variables so bumps are plain -D flags. If the repo ever
# needs auth again, export NATIVE_DEPS_READ_TOKEN and it is sent as a
# Bearer-style Forgejo token header.

set(OWON_WX_BASE_URL "https://git.h.netwake.cc/tlamy/native-deps/releases/download"
    CACHE STRING "Base URL for native-deps release downloads")

if(APPLE)
    set(OWON_WX_TAG "wxwidgets-3.3.4-r1" CACHE STRING "native-deps release tag for wxWidgets")
    set(OWON_WX_ASSET "wxwidgets-3.3.4-r1-macos-universal.tar.gz" CACHE STRING "wxWidgets asset name")
    set(OWON_WX_SHA256 "db5512b31536016a7784cf4055b3a0131232c5539885c39491d82b9e8af6d501"
        CACHE STRING "SHA-256 of the wxWidgets asset")
elseif(UNIX)
    set(OWON_WX_TAG "wxwidgets-3.3.4-r2" CACHE STRING "native-deps release tag for wxWidgets")
    set(OWON_WX_ASSET "wxwidgets-3.3.4-r2-linux-amd64-trixie.tar.gz" CACHE STRING "wxWidgets asset name")
    set(OWON_WX_SHA256 "b2ad8e681256b6102c32a6c713bfebff1af2bba6e277fc7c276fcf8792ae214f"
        CACHE STRING "SHA-256 of the wxWidgets asset")
else()
    message(FATAL_ERROR "No prebuilt wxWidgets pin for this platform; configure with -DOWON_USE_SYSTEM_WX=ON")
endif()

set(OWON_WX_DIR "${CMAKE_BINARY_DIR}/_wx-prebuilt")
set(OWON_WX_ARCHIVE "${OWON_WX_DIR}/${OWON_WX_ASSET}")
set(WX_PREBUILT_PREFIX "${OWON_WX_DIR}/wxroot/prefix")

if(NOT EXISTS "${WX_PREBUILT_PREFIX}/bin/wx-config")
    file(MAKE_DIRECTORY "${OWON_WX_DIR}")
    set(_wx_have_archive FALSE)
    if(EXISTS "${OWON_WX_ARCHIVE}")
        file(SHA256 "${OWON_WX_ARCHIVE}" _wx_have)
        string(TOLOWER "${_wx_have}" _wx_have)
        string(TOLOWER "${OWON_WX_SHA256}" _wx_want)
        if(_wx_have STREQUAL _wx_want)
            set(_wx_have_archive TRUE)
        else()
            message(STATUS "Cached wxWidgets archive has wrong hash, re-downloading")
            file(REMOVE "${OWON_WX_ARCHIVE}")
        endif()
    endif()
    if(NOT _wx_have_archive)
        message(STATUS "Downloading wxWidgets ${OWON_WX_TAG}/${OWON_WX_ASSET}")
        set(_wx_url "${OWON_WX_BASE_URL}/${OWON_WX_TAG}/${OWON_WX_ASSET}")
        set(_wx_headers)
        if(DEFINED ENV{NATIVE_DEPS_READ_TOKEN} AND NOT "$ENV{NATIVE_DEPS_READ_TOKEN}" STREQUAL "")
            set(_wx_headers HTTPHEADER "Authorization: token $ENV{NATIVE_DEPS_READ_TOKEN}")
        endif()
        find_program(_wx_curl curl)
        set(_wx_dl_ok FALSE)
        # The Forgejo host is on the LAN; its route can flap, so retry.
        foreach(_wx_try RANGE 1 4)
            if(_wx_curl)
                set(_wx_curl_headers)
                if(DEFINED ENV{NATIVE_DEPS_READ_TOKEN} AND NOT "$ENV{NATIVE_DEPS_READ_TOKEN}" STREQUAL "")
                    set(_wx_curl_headers -H "Authorization: token $ENV{NATIVE_DEPS_READ_TOKEN}")
                endif()
                execute_process(
                    COMMAND "${_wx_curl}" --fail --silent --show-error --location
                        --retry 2 --retry-all-errors --connect-timeout 20
                        ${_wx_curl_headers} -o "${OWON_WX_ARCHIVE}" "${_wx_url}"
                    RESULT_VARIABLE _wx_curl_rc)
                if(_wx_curl_rc EQUAL 0)
                    file(SHA256 "${OWON_WX_ARCHIVE}" _wx_got)
                    string(TOLOWER "${_wx_got}" _wx_got)
                    string(TOLOWER "${OWON_WX_SHA256}" _wx_want)
                    if(_wx_got STREQUAL _wx_want)
                        set(_wx_dl_ok TRUE)
                    else()
                        message(STATUS "wxWidgets download hash mismatch (try ${_wx_try}), retrying")
                        file(REMOVE "${OWON_WX_ARCHIVE}")
                    endif()
                else()
                    message(STATUS "wxWidgets download failed (try ${_wx_try}, curl rc ${_wx_curl_rc}), retrying")
                    file(REMOVE "${OWON_WX_ARCHIVE}")
                endif()
            else()
                file(DOWNLOAD "${_wx_url}" "${OWON_WX_ARCHIVE}"
                    EXPECTED_HASH SHA256=${OWON_WX_SHA256}
                    ${_wx_headers}
                    STATUS _wx_status)
                list(GET _wx_status 0 _wx_code)
                if(_wx_code EQUAL 0)
                    set(_wx_dl_ok TRUE)
                else()
                    list(GET _wx_status 1 _wx_msg)
                    message(STATUS "wxWidgets download failed (try ${_wx_try}: ${_wx_msg}), retrying")
                    file(REMOVE "${OWON_WX_ARCHIVE}")
                endif()
            endif()
            if(_wx_dl_ok)
                break()
            endif()
            execute_process(COMMAND sleep 3)
        endforeach()
        if(NOT _wx_dl_ok)
            file(REMOVE "${OWON_WX_ARCHIVE}")
            message(FATAL_ERROR "wxWidgets download failed after 4 tries: ${_wx_url}")
        endif()
    endif()
    file(REMOVE_RECURSE "${OWON_WX_DIR}/wxroot")
    file(MAKE_DIRECTORY "${OWON_WX_DIR}/wxroot")
    file(ARCHIVE_EXTRACT INPUT "${OWON_WX_ARCHIVE}" DESTINATION "${OWON_WX_DIR}/wxroot")
    if(NOT EXISTS "${WX_PREBUILT_PREFIX}/bin/wx-config")
        message(FATAL_ERROR "wxWidgets archive has unexpected layout (no prefix/bin/wx-config)")
    endif()
endif()

# Wrapper: the shipped wx-config has the builder's absolute prefix baked in,
# but honours --prefix=, so force it onto our extracted copy.
set(OWON_WX_CONFIG "${OWON_WX_DIR}/wx-config" CACHE INTERNAL "wx-config wrapper for the prebuilt wxWidgets")
file(WRITE "${OWON_WX_CONFIG}"
    "#!/bin/sh\n# Generated by cmake/wx-prebuilt.cmake; do not edit.\nexec \"${WX_PREBUILT_PREFIX}/bin/wx-config\" --prefix=\"${WX_PREBUILT_PREFIX}\" \"$@\"\n")
execute_process(COMMAND chmod +x "${OWON_WX_CONFIG}")
