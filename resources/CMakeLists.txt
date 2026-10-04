# Application configuration and image resources.
#
# SVG icons are based on Google Material Symbols Outlined and are stored
# locally so that the application has no runtime dependency on online assets.
qt_add_resources(appReqDeck "application_resources"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"

    FILES
        qtquickcontrols2.conf

        # Application branding: multi-size window/taskbar icon and the logo shown
        # on the launcher start page and in the About dialog. Source PNGs live
        # under images/app/source/ (the .exe icon is embedded through the
        # generated version resource, not as a runtime resource).
        images/app/ReqDeck.ico
        images/app/ReqDeckLogo.png

        # Menu, toolbar, and panel icons.
        images/svg/article.svg
        images/svg/close.svg
        images/svg/code.svg
        images/svg/content_copy.svg
        images/svg/dark_mode.svg
        images/svg/description.svg
        images/svg/exit_to_app.svg
        images/svg/folder.svg
        images/svg/folder_open.svg
        images/svg/history.svg
        images/svg/info.svg
        images/svg/light_mode.svg
        images/svg/note_add.svg
        images/svg/play_arrow.svg
        images/svg/save.svg
        images/svg/save_as.svg
        images/svg/settings.svg
        images/svg/stop.svg
        images/svg/update.svg

        # Country flag for the UI-language selector (4:3 SVG; public-domain flag
        # artwork). The file name is the ISO country part of the locale: en_GB->gb.
        images/flags/gb.svg
)

# License and notice documents shown by the Help > About dialog. The canonical
# copies live at the project root (single source of truth for both source
# distribution and REUSE); they are embedded here so the About dialog has no
# runtime dependency on any deployed folder. BASE is the project root, so the
# files keep their LICENSES/ path under the /licenses resource prefix.
qt_add_resources(appReqDeck "license_resources"
    PREFIX "/licenses"
    BASE "${CMAKE_SOURCE_DIR}"

    FILES
        "${CMAKE_SOURCE_DIR}/NOTICE"
        "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${CMAKE_SOURCE_DIR}/LICENSES/GPL-3.0-or-later.txt"
        "${CMAKE_SOURCE_DIR}/LICENSES/LGPL-3.0-only.txt"
        "${CMAKE_SOURCE_DIR}/LICENSES/Apache-2.0.txt"
)
