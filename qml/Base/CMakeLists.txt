# Reusable application controls. Components in this module must not import
# application-only C++ modules (Cpp.*): the QML smoke test instantiates them
# without the application's context.
qt_add_library(Base STATIC)

# Shared accent tokens and chrome palette, bound to the window theme by Main.qml.
set_source_files_properties(BsTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Base
    URI Base
    CLASS_NAME BasePlugin
    QML_FILES
        BsLanguageSelector.qml
        BsLogPanel.qml
        BsMenuBar.qml
        BsMenuItem.qml
        BsNotificationBanner.qml
        BsPane.qml
        BsStatusBar.qml
        BsTheme.qml
        BsTopBarActions.qml
)

target_link_libraries(Base
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
)
