# Static wxWidgets from the native-deps Forgejo releases.
#
# Downloads (once per build dir) the pinned tarball, verifies its SHA-256,
# extracts it, and queries the prebuilt wx-config directly, exposing:
#   OWON_WX_CFLAGS - compile flags for the prebuilt (wx-config --cxxflags)
#   OWON_WX_LIBS   - link line for core+base (wx-config --libs core base)
#   OWON_WX_CONFIG - wrapper script forcing wx-config onto that prefix
#
# NOTE: we deliberately do NOT use find_package(wxWidgets) here. wx 3.3's
# multi-config wx-config answers the --static=yes/no selector probes of
# FindwxWidgets with exit 0 but no usable output for a static-only prefix,
# so the module ends up querying "--static=no --libs" and reports
# "Could NOT find wxWidgets (missing: wxWidgets_LIBRARIES)" on several
# CMake versions. Bare "--cxxflags" / "--libs" queries work everywhere.
#
# Pins are CACHE variables so bumps are plain -D flags. If the repo ever
# needs auth again, export NATIVE_DEPS_READ_TOKEN and it is sent as a
# Forgejo token header.

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

# Resolve a usable wx-config script. bin/wx-config is sometimes a dangling
# absolute symlink into the builder's prefix (EXISTS is false for those),
# so fall back to the real per-config script under lib/wx/config/.
macro(_owon_resolve_wx_config out_var)
    set(${out_var} "${WX_PREBUILT_PREFIX}/bin/wx-config")
    if(NOT EXISTS "${${out_var}}")
        file(GLOB _wx_lib_configs "${WX_PREBUILT_PREFIX}/lib/wx/config/*")
        list(LENGTH _wx_lib_configs _wx_lib_configs_n)
        if(_wx_lib_configs_n EQUAL 0)
            set(${out_var} "")
        else()
            list(GET _wx_lib_configs 0 ${out_var})
        endif()
        unset(_wx_lib_configs)
        unset(_wx_lib_configs_n)
    endif()
endmacro()

_owon_resolve_wx_config(OWON_WX_CONFIG_SCRIPT)
if(OWON_WX_CONFIG_SCRIPT STREQUAL "")
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
    _owon_resolve_wx_config(OWON_WX_CONFIG_SCRIPT)
    if(OWON_WX_CONFIG_SCRIPT STREQUAL "")
        message(FATAL_ERROR "wxWidgets archive has unexpected layout (no usable wx-config under prefix/)")
    endif()
    # Repair a dangling bin/wx-config with a relative link so the prefix
    # looks normal to humans and to tools that expect bin/wx-config.
    if(NOT EXISTS "${WX_PREBUILT_PREFIX}/bin/wx-config")
        get_filename_component(_wx_cfg_name "${OWON_WX_CONFIG_SCRIPT}" NAME)
        file(REMOVE "${WX_PREBUILT_PREFIX}/bin/wx-config")
        file(CREATE_LINK "../lib/wx/config/${_wx_cfg_name}"
            "${WX_PREBUILT_PREFIX}/bin/wx-config" SYMBOLIC)
        unset(_wx_cfg_name)
    endif()
endif()

# Wrapper: the shipped wx-config has the builder's absolute prefix baked in,
# but honours --prefix=, so force it onto our extracted copy. It execs the
# resolved real script, never a dangling bin/ symlink.
set(OWON_WX_CONFIG "${OWON_WX_DIR}/wx-config" CACHE INTERNAL "wx-config wrapper for the prebuilt wxWidgets")
file(WRITE "${OWON_WX_CONFIG}"
    "#!/bin/sh\n# Generated by cmake/wx-prebuilt.cmake; do not edit.\nexec \"${OWON_WX_CONFIG_SCRIPT}\" --prefix=\"${WX_PREBUILT_PREFIX}\" \"$@\"\n")
execute_process(COMMAND chmod +x "${OWON_WX_CONFIG}")

# Query the prebuilt directly (see note at the top about find_package).
execute_process(
    COMMAND "${OWON_WX_CONFIG}" --cxxflags
    OUTPUT_VARIABLE _wx_cxxflags
    ERROR_VARIABLE _wx_cxxflags_err
    RESULT_VARIABLE _wx_cxxflags_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _wx_cxxflags_rc EQUAL 0)
    message(FATAL_ERROR "prebuilt wx-config --cxxflags failed: ${_wx_cxxflags_err}")
endif()
separate_arguments(OWON_WX_CFLAGS UNIX_COMMAND "${_wx_cxxflags}")
unset(_wx_cxxflags)
unset(_wx_cxxflags_err)
unset(_wx_cxxflags_rc)

execute_process(
    COMMAND "${OWON_WX_CONFIG}" --libs core base
    OUTPUT_VARIABLE _wx_libs
    ERROR_VARIABLE _wx_libs_err
    RESULT_VARIABLE _wx_libs_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _wx_libs_rc EQUAL 0)
    message(FATAL_ERROR "prebuilt wx-config --libs failed: ${_wx_libs_err}")
endif()
if(_wx_libs STREQUAL "")
    message(FATAL_ERROR "prebuilt wx-config --libs returned no libraries")
endif()
separate_arguments(OWON_WX_LIBS UNIX_COMMAND "${_wx_libs}")
unset(_wx_libs)
unset(_wx_libs_err)
unset(_wx_libs_rc)
# Keep "-framework Foo" pairs together: as separate list items CMake would
# treat the bare framework name as a library (-lFoo). Join them instead.
set(_wx_joined_libs)
set(_wx_lib_i 0)
list(LENGTH OWON_WX_LIBS _wx_libs_n)
while(_wx_lib_i LESS _wx_libs_n)
    list(GET OWON_WX_LIBS ${_wx_lib_i} _wx_lib_item)
    if(_wx_lib_item STREQUAL "-framework")
        math(EXPR _wx_lib_i "${_wx_lib_i} + 1")
        list(GET OWON_WX_LIBS ${_wx_lib_i} _wx_fw_name)
        list(APPEND _wx_joined_libs "-framework ${_wx_fw_name}")
        unset(_wx_fw_name)
    else()
        list(APPEND _wx_joined_libs "${_wx_lib_item}")
    endif()
    unset(_wx_lib_item)
    math(EXPR _wx_lib_i "${_wx_lib_i} + 1")
endwhile()
set(OWON_WX_LIBS "${_wx_joined_libs}")
unset(_wx_joined_libs)
unset(_wx_lib_i)
unset(_wx_libs_n)
# The prebuilt bakes the builder's Xcode SDK paths
# (.../MacOSX.sdk/usr/lib/*.tbd) into --libs, but runners may have Xcode
# installed elsewhere. These are all system libs, so link them by name.
set(_wx_remapped_libs)
foreach(_wx_lib_item IN LISTS OWON_WX_LIBS)
    if(_wx_lib_item MATCHES "^(.*/)?usr/lib/lib([^/]+)\\.tbd$")
        list(APPEND _wx_remapped_libs "-l${CMAKE_MATCH_2}")
    else()
        list(APPEND _wx_remapped_libs "${_wx_lib_item}")
    endif()
endforeach()
set(OWON_WX_LIBS "${_wx_remapped_libs}")
unset(_wx_remapped_libs)
# The trixie-built wx links -lwebpdecoder, but some distros (e.g. Ubuntu
# 22.04) folded the decoder back into libwebp and ship no such library.
# Drop it when the toolchain cannot resolve it; -lwebp (also on the link
# line) provides the same symbols.
list(FIND OWON_WX_LIBS "-lwebpdecoder" _wx_has_webpdecoder)
if(NOT _wx_has_webpdecoder EQUAL -1)
    find_library(_wx_webpdecoder_lib NAMES webpdecoder)
    if(_wx_webpdecoder_lib MATCHES "-NOTFOUND$")
        list(REMOVE_ITEM OWON_WX_LIBS "-lwebpdecoder")
        message(STATUS "libwebpdecoder not found, linking decoder symbols via libwebp")
    endif()
    unset(_wx_webpdecoder_lib)
endif()
unset(_wx_has_webpdecoder)
