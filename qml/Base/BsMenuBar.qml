// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsMenuBar
    \inqmlmodule Base
    \brief Provides the application menu bar and user action signals.

    The menu bar is a pure view: the host supplies the workspace state through
    properties (\l hasWorkspace, \l workspaceDirty, \l recentWorkspaces) and
    reacts to the signals. It reads no application context property, so it can be
    created in isolation, for example by the QML smoke test.

    Request actions that the HTTP features of a later milestone provide are
    present but disabled, so the menu layout is final from the start.
*/
MenuBar {
    id: appMenuBar

    property string helpTopic: "menu"

    /*! Whether the menu should describe the current theme as dark. */
    property bool darkTheme: false

    /*! Whether the log panel is currently shown, used to word the menu item. */
    property bool logPanelVisible: false

    /*! Whether a workspace is open; enables the workspace-scoped actions. */
    property bool hasWorkspace: false

    /*! Whether the open workspace has unsaved changes; enables Save. */
    property bool workspaceDirty: false

    /*!
        Recent-workspace rows, newest first, as provided by
        \c WorkspaceManager::recentWorkspaces (maps with \c displayName,
        \c path, and \c available).
    */
    property var recentWorkspaces: []

    /*! Product name used in the About item. */
    property string appName: ""

    /*! Whether an item is selected in the collection. */
    property bool hasSelection: false

    /*! Whether a request is open in the editor. */
    property bool hasRequest: false

    /*! Whether the open request can be sent. */
    property bool canSend: false

    /*! Whether a running request can be cancelled. */
    property bool canCancel: false

    /*! Emitted when the user selects New Workspace. */
    signal newWorkspaceRequested()

    /*! Emitted when the user selects Open Workspace. */
    signal openWorkspaceRequested()

    /*! Emitted when the user selects the recent workspace at \a index. */
    signal openRecentRequested(int index)

    /*! Emitted when the user selects Save. */
    signal saveWorkspaceRequested()

    /*! Emitted when the user selects Save As. */
    signal saveWorkspaceAsRequested()

    /*! Emitted when the user selects Close Workspace. */
    signal closeWorkspaceRequested()

    /*! Emitted when the user opens the application settings. */
    signal settingsRequested()

    /*! Emitted to copy the open request as a cURL command. */
    signal copyCurlRequested()

    /*! Emitted to import a cURL command. */
    signal importCurlRequested()

    /*! Emitted when the user selects Quit. */
    signal quitRequested()

    /*! Emitted when the user asks to switch between the light and dark themes. */
    signal themeToggleRequested()

    /*! Emitted when the user asks to show or hide the log panel. */
    signal logPanelToggleRequested()

    /*! Emitted when the user selects Check for Updates. */
    signal checkForUpdatesRequested()

    /*! Emitted when the user opens the About dialog. */
    signal aboutRequested()

    /*! Emitted to add a request. */
    signal newRequestRequested()

    /*! Emitted to add a folder. */
    signal newFolderRequested()

    /*! Emitted to duplicate the selected item. */
    signal duplicateRequested()

    /*! Emitted to send the open request. */
    signal sendRequested()

    /*! Emitted to cancel the running request. */
    signal cancelRequested()

    background: Rectangle {
        implicitHeight: 40
        color: BsTheme.menuBandColor
    }

    // A highlighted title gets a light accent fill; the title whose menu is open
    // also gets a thin accent underline.
    delegate: MenuBarItem {
        id: menuBarItem

        background: Rectangle {
            implicitWidth: 40
            implicitHeight: 40
            color: menuBarItem.highlighted ? BsTheme.menuHighlightColor : "transparent"

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                height: 2
                radius: 1
                color: BsTheme.accentLineColor
                visible: menuBarItem.menu !== null && menuBarItem.menu.visible
            }
        }
    }

    Menu {
        property string helpTopic: "menu"

        title: qsTr("&File")
        // Menus do not grow with their items, and these name their shortcuts.
        width: 300
        // The entries of submenus are created from this delegate.
        delegate: BsMenuItem {}

        BsMenuItem {
            text: qsTr("&New Workspace…")
            icon.source: "qrc:/images/svg/note_add.svg"
            onTriggered: appMenuBar.newWorkspaceRequested()
        }

        BsMenuItem {
            text: qsTr("&Open Workspace…")
            shortcutText: "Ctrl+O"
            icon.source: "qrc:/images/svg/folder_open.svg"
            onTriggered: appMenuBar.openWorkspaceRequested()
        }

        Menu {
            id: recentMenu

            property string helpTopic: "menu"

            // A submenu entry copies the whole Menu.icon, so an unset size would
            // replace the style's 24 px item icon with the SVG's intrinsic size.
            // Every submenu therefore states the menu item icon size explicitly.
            title: qsTr("Open &Recent")
            icon.source: "qrc:/images/svg/history.svg"
            icon.width: 24
            icon.height: 24
            enabled: appMenuBar.recentWorkspaces.length > 0

            Instantiator {
                model: appMenuBar.recentWorkspaces

                delegate: BsMenuItem {
                    required property int index
                    required property var modelData

                    text: modelData.displayName.length > 0
                          ? modelData.displayName : modelData.path
                    icon.source: "qrc:/images/svg/description.svg"
                    enabled: modelData.available
                    onTriggered: appMenuBar.openRecentRequested(index)
                }

                onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => recentMenu.removeItem(object)
            }
        }

        MenuSeparator {}

        BsMenuItem {
            text: qsTr("&Save")
            shortcutText: "Ctrl+S"
            icon.source: "qrc:/images/svg/save.svg"
            enabled: appMenuBar.hasWorkspace && appMenuBar.workspaceDirty
            onTriggered: appMenuBar.saveWorkspaceRequested()
        }

        BsMenuItem {
            text: qsTr("Save &As…")
            shortcutText: "Ctrl+Shift+S"
            icon.source: "qrc:/images/svg/save_as.svg"
            enabled: appMenuBar.hasWorkspace
            onTriggered: appMenuBar.saveWorkspaceAsRequested()
        }

        BsMenuItem {
            text: qsTr("&Close Workspace")
            icon.source: "qrc:/images/svg/close.svg"
            enabled: appMenuBar.hasWorkspace
            onTriggered: appMenuBar.closeWorkspaceRequested()
        }

        MenuSeparator {}

        BsMenuItem {
            text: qsTr("&Import cURL…")
            icon.source: "qrc:/images/svg/code.svg"
            enabled: appMenuBar.hasWorkspace
            onTriggered: appMenuBar.importCurlRequested()
        }

        MenuSeparator {}

        BsMenuItem {
            text: qsTr("Se&ttings…")
            icon.source: "qrc:/images/svg/settings.svg"
            onTriggered: appMenuBar.settingsRequested()
        }

        MenuSeparator {}

        BsMenuItem {
            text: qsTr("&Quit")
            shortcutText: "Ctrl+Q"
            icon.source: "qrc:/images/svg/exit_to_app.svg"
            onTriggered: appMenuBar.quitRequested()
        }
    }

    Menu {
        property string helpTopic: "menu"

        title: qsTr("&Request")
        width: 300
        delegate: BsMenuItem {}

        BsMenuItem {
            text: qsTr("&New Request")
            shortcutText: "Ctrl+N"
            icon.source: "qrc:/images/svg/note_add.svg"
            enabled: appMenuBar.hasWorkspace
            onTriggered: appMenuBar.newRequestRequested()
        }

        BsMenuItem {
            text: qsTr("New &Folder")
            icon.source: "qrc:/images/svg/folder.svg"
            enabled: appMenuBar.hasWorkspace
            onTriggered: appMenuBar.newFolderRequested()
        }

        MenuSeparator {}

        BsMenuItem {
            text: qsTr("&Send")
            shortcutText: "Ctrl+Enter"
            icon.source: "qrc:/images/svg/play_arrow.svg"
            enabled: appMenuBar.canSend
            onTriggered: appMenuBar.sendRequested()
        }

        BsMenuItem {
            text: qsTr("&Cancel")
            icon.source: "qrc:/images/svg/stop.svg"
            enabled: appMenuBar.canCancel
            onTriggered: appMenuBar.cancelRequested()
        }

        MenuSeparator {}

        BsMenuItem {
            text: qsTr("&Duplicate")
            icon.source: "qrc:/images/svg/content_copy.svg"
            enabled: appMenuBar.hasWorkspace && appMenuBar.hasSelection
            onTriggered: appMenuBar.duplicateRequested()
        }

        BsMenuItem {
            text: qsTr("Copy as c&URL")
            icon.source: "qrc:/images/svg/code.svg"
            enabled: appMenuBar.hasRequest
            onTriggered: appMenuBar.copyCurlRequested()
        }
    }

    Menu {
        property string helpTopic: "menu"

        title: qsTr("&View")
        delegate: BsMenuItem {}

        BsMenuItem {
            text: appMenuBar.darkTheme
                  ? qsTr("Switch to &Light Theme")
                  : qsTr("Switch to &Dark Theme")
            icon.source: appMenuBar.darkTheme ? "qrc:/images/svg/light_mode.svg"
                                              : "qrc:/images/svg/dark_mode.svg"
            onTriggered: appMenuBar.themeToggleRequested()
        }

        BsMenuItem {
            text: appMenuBar.logPanelVisible ? qsTr("Hide &Log Panel")
                                             : qsTr("Show &Log Panel")
            icon.source: "qrc:/images/svg/article.svg"
            onTriggered: appMenuBar.logPanelToggleRequested()
        }
    }

    Menu {
        property string helpTopic: "menu"

        title: qsTr("&Help")
        delegate: BsMenuItem {}

        BsMenuItem {
            text: qsTr("&Check for Updates…")
            icon.source: "qrc:/images/svg/update.svg"
            onTriggered: appMenuBar.checkForUpdatesRequested()
        }

        MenuSeparator {}

        BsMenuItem {
            text: appMenuBar.appName.length > 0 ? qsTr("&About %1…").arg(appMenuBar.appName)
                                                : qsTr("&About…")
            icon.source: "qrc:/images/svg/info.svg"
            onTriggered: appMenuBar.aboutRequested()
        }
    }
}
