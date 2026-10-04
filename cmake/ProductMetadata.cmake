# ProductMetadata.cmake
#
# Loads the single product-metadata source (packaging/product.json) and exposes
# its fields as PRODUCT_* CMake variables in the including scope. Include this
# BEFORE project() so that project(... VERSION ${PRODUCT_VERSION} ...) and the
# generated productinfo.h / *.rc files all draw from the same file.
#
# The JSON is intentionally the only place the product name and version are
# defined; both the CMake build and the PowerShell packaging scripts read it, so
# the two never drift apart.

# Resolve packaging/product.json relative to this module (cmake/ -> project root).
get_filename_component(_reqdeck_project_root "${CMAKE_CURRENT_LIST_DIR}" DIRECTORY)
set(PRODUCT_METADATA_FILE "${_reqdeck_project_root}/packaging/product.json"
    CACHE FILEPATH "Path to the product-metadata JSON single source")

if(NOT EXISTS "${PRODUCT_METADATA_FILE}")
    message(FATAL_ERROR "ProductMetadata: metadata file not found: ${PRODUCT_METADATA_FILE}")
endif()

file(READ "${PRODUCT_METADATA_FILE}" _reqdeck_product_json)

# Re-run CMake when the metadata changes; otherwise an incremental build keeps
# the version and names baked into the previously generated .rc/header files.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PRODUCT_METADATA_FILE}")

# Read one string field from the JSON into the named parent-scope variable.
function(_reqdeck_json_get out_var)
    string(JSON _value ERROR_VARIABLE _err GET "${_reqdeck_product_json}" ${ARGN})
    if(_err)
        message(FATAL_ERROR "ProductMetadata: missing field '${ARGN}' in ${PRODUCT_METADATA_FILE}: ${_err}")
    endif()
    set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

_reqdeck_json_get(PRODUCT_DISPLAY_NAME          displayName)
_reqdeck_json_get(PRODUCT_IDENTIFIER            identifier)
_reqdeck_json_get(PRODUCT_ORG_DOMAIN            orgDomain)
_reqdeck_json_get(PRODUCT_VERSION               version)
_reqdeck_json_get(PRODUCT_PUBLISHER             publisher)
_reqdeck_json_get(PRODUCT_COPYRIGHT             copyright)
_reqdeck_json_get(PRODUCT_HOMEPAGE              homepage)
_reqdeck_json_get(PRODUCT_EXE_NAME              exeName)
_reqdeck_json_get(PRODUCT_INSTALL_DIR_NAME      installDirName)
_reqdeck_json_get(PRODUCT_COMPONENT_ID          componentId)
_reqdeck_json_get(PRODUCT_MAINTENANCE_TOOL_NAME maintenanceToolName)
_reqdeck_json_get(PRODUCT_ARTIFACT_BASE         artifactBase)

# In-app update-check source (GitHub Releases). The feature is gated by
# update.enabled so the network client can ship fully implemented but stay off
# until the release server is ready. string(JSON) yields ON/OFF for a JSON
# boolean; normalize it to a C++ bool literal for the generated header.
_reqdeck_json_get(PRODUCT_UPDATE_ENABLED        update enabled)
_reqdeck_json_get(PRODUCT_UPDATE_RELEASES_API   update releasesApiUrl)
_reqdeck_json_get(PRODUCT_UPDATE_RELEASES_PAGE  update releasesPageUrl)
if(PRODUCT_UPDATE_ENABLED)
    set(PRODUCT_UPDATE_ENABLED_BOOL "true")
else()
    set(PRODUCT_UPDATE_ENABLED_BOOL "false")
endif()

# Split the semantic version into numeric parts for the Windows VERSIONINFO
# resource (major,minor,patch,build).
if(NOT PRODUCT_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "ProductMetadata: version '${PRODUCT_VERSION}' is not MAJOR.MINOR.PATCH")
endif()
set(PRODUCT_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(PRODUCT_VERSION_MINOR "${CMAKE_MATCH_2}")
set(PRODUCT_VERSION_PATCH "${CMAKE_MATCH_3}")
set(PRODUCT_VERSION_BUILD "0")

# Absolute, forward-slash path to the application .exe icon (embedded through
# reqdeck_add_windows_version_info(); see cmake/WindowsVersionInfo.cmake).
set(PRODUCT_ICON_PATH "${_reqdeck_project_root}/resources/images/app/ReqDeck.ico")
