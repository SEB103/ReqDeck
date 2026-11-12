# Reusable visual components form their own QML module.
add_subdirectory(Base)

# Application-level QML module. Main.qml contains behavior and signal handling;
# MainScreen.ui.qml contains the declarative visual composition.
qt_add_library(MainQml STATIC)

qt_add_qml_module(MainQml
    URI ReqDeck
    CLASS_NAME ReqDeckPlugin
    QML_FILES
        AboutDialog.qml
        CollectionPane.qml
        CookieDialog.qml
        CurlImportDialog.qml
        EnvironmentDialog.qml
        FolderSettingsDialog.qml
        FormDataTable.qml
        HistoryPane.qml
        ImportDialog.qml
        LauncherScreen.qml
        Main.qml
        MainScreen.ui.qml
        RequestPane.qml
        RequestTabBar.qml
        ResponsePane.qml
        UpdateDialog.qml
)

target_link_libraries(MainQml
    PRIVATE
        Base
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
)
