# WindowsVersionInfo.cmake
#
# reqdeck_add_windows_version_info(<target>
#     DESCRIPTION <text>
#     [ICON <path>])
#
# Generates a Windows resource script from cmake/versioninfo.rc.in and adds it
# to <target>, so the built .exe carries a VERSIONINFO block (product name,
# version, publisher, copyright) and optionally the application icon. All
# values except DESCRIPTION come from packaging/product.json through the
# PRODUCT_* variables loaded by ProductMetadata.cmake.
#
# Every shipped executable must call this: the code-signing service requires
# ProductName and ProductVersion metadata on each signed binary, and Explorer
# shows the values in the file properties. A no-op on non-Windows platforms.

function(reqdeck_add_windows_version_info target)
    if(NOT WIN32)
        return()
    endif()

    cmake_parse_arguments(_vi "" "DESCRIPTION;ICON" "" ${ARGN})
    if(NOT _vi_DESCRIPTION)
        message(FATAL_ERROR "reqdeck_add_windows_version_info(${target}): DESCRIPTION is required")
    endif()
    if(_vi_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "reqdeck_add_windows_version_info(${target}): unexpected arguments: ${_vi_UNPARSED_ARGUMENTS}")
    endif()

    get_target_property(_vi_output_name ${target} OUTPUT_NAME)
    if(NOT _vi_output_name)
        set(_vi_output_name "${target}")
    endif()

    # Variables consumed by versioninfo.rc.in (@ONLY substitution).
    set(PRODUCT_RC_EXE_NAME         "${_vi_output_name}")
    set(PRODUCT_RC_FILE_DESCRIPTION "${_vi_DESCRIPTION}")
    if(_vi_ICON)
        # rc.exe accepts forward slashes; this avoids backslash escaping.
        file(TO_CMAKE_PATH "${_vi_ICON}" _vi_icon_path)
        set(PRODUCT_RC_ICON_STATEMENT "IDI_ICON1 ICON \"${_vi_icon_path}\"")
    else()
        set(PRODUCT_RC_ICON_STATEMENT "// (no icon)")
    endif()

    set(_vi_rc "${REQDECK_GENERATED_DIR}/${target}.rc")
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/versioninfo.rc.in"
        "${_vi_rc}"
        @ONLY
    )
    target_sources(${target} PRIVATE "${_vi_rc}")
endfunction()
