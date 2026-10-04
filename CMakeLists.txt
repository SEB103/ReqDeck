cmake_minimum_required(VERSION 3.21)

# Load the single product-metadata source (packaging/product.json) before
# project() so the product version and identity come from one place.
include(cmake/ProductMetadata.cmake)

project(ReqDeck
    VERSION ${PRODUCT_VERSION}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Generate compile-time product metadata (productinfo.h) from the single source
# in packaging/product.json. The header is included by the app layer to drive
# the display name, organization, copyright and version.
set(REQDECK_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/productinfo.h.in"
    "${REQDECK_GENERATED_DIR}/productinfo.h"
    @ONLY
)

find_package(Qt6 6.11 REQUIRED COMPONENTS
    Core
    Gui
    LinguistTools
    Network
    Qml
    Quick
    QuickControls2
    QuickDialogs2
    Svg
)

qt_standard_project_setup(
    REQUIRES 6.11
    I18N_SOURCE_LANGUAGE en
    I18N_TRANSLATED_LANGUAGES
        en_GB
)

include(CTest)
include(GNUInstallDirs)
include(cmake/WindowsVersionInfo.cmake)

# Keep all generated QML module metadata under one predictable build-tree root.
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

# Build the reusable C++ layers and both QML modules.
add_subdirectory(src)
add_subdirectory(qml)

qt_add_executable(appReqDeck
    app/main.cpp
    app/appcore.cpp
    app/appcore.h
    app/appengine.cpp
    app/appengine.h
    app/appinfo.cpp
    app/appinfo.h
    app/licensemodel.cpp
    app/licensemodel.h
    app/localecontroller.cpp
    app/localecontroller.h
    app/updatecontroller.cpp
    app/updatecontroller.h
)

# Application-level style configuration, branding, icons, and license texts.
add_subdirectory(resources)

target_include_directories(appReqDeck
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/app"
        "${REQDECK_GENERATED_DIR}"
)

target_link_libraries(appReqDeck
    PRIVATE
        ReqDeckQmlApi
        ReqDeckFramework
        MainQmlplugin
        Baseplugin
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Svg
)

# Ensure statically linked QML plugins are retained and imported correctly.
qt_import_qml_plugins(appReqDeck)

# The CMake target keeps the conventional app* name; the shipped executable is
# named after packaging/product.json (exeName), e.g. ReqDeck.exe.
set_target_properties(appReqDeck PROPERTIES
    OUTPUT_NAME "${PRODUCT_EXE_NAME}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Embed the application icon and version metadata into the Windows .exe (used by
# Explorer, the taskbar/Alt+Tab and Desktop/Start Menu shortcuts). The generated
# .rc takes its VERSIONINFO block from packaging/product.json; MSVC compiles it
# automatically.
reqdeck_add_windows_version_info(appReqDeck
    DESCRIPTION "${PRODUCT_DISPLAY_NAME}"
    ICON "${PRODUCT_ICON_PATH}"
)

# The UI currently ships in English only; the catalog keeps the translation
# plumbing (LocaleController, live retranslation) ready for further languages.
qt_add_translations(appReqDeck
    TS_FILES
        translations/ReqDeck_en_GB.ts
    RESOURCE_PREFIX "/translations"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS appReqDeck
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

qt_generate_deploy_qml_app_script(
    TARGET appReqDeck
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT "${deploy_script}")

# Ship the licensing materials required to distribute the binaries (own GPL
# license plus third-party notices and license texts).
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/licenses"
)
