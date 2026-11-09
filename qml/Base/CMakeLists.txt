# Reusable application controls. Components in this module must not import
# application-only C++ modules (Cpp.*): the QML smoke test instantiates them
# without the application's context.
qt_add_library(Base STATIC)

# Shared accent tokens and chrome palette, bound to the window theme by Main.qml.
set_source_files_properties(BsTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Finds {{variable}} references for fields that cannot be highlighted.
set_source_files_properties(BsVariables.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Base
    URI Base
    CLASS_NAME BasePlugin
    QML_FILES
        BsCommandPalette.qml
        BsEnvironmentSelector.qml
        BsHelpPanel.qml
        BsInlineTitle.qml
        BsKeyValueTable.qml
        BsLanguageSelector.qml
        BsLineNumbers.qml
        BsLogPanel.qml
        BsMenuBar.qml
        BsMenuItem.qml
        BsNotificationBanner.qml
        BsPane.qml
        BsSegmentedControl.qml
        BsSingleLineEdit.qml
        BsStatusBar.qml
        BsStatusChip.qml
        BsTabButton.qml
        BsTheme.qml
        BsTopBarActions.qml
        BsUnknownVariablesMark.qml
        BsVariableCompleter.qml
        BsVariables.qml
)

target_link_libraries(Base
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
)
