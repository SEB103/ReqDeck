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
        title: qsTr("&File")
        // The entries of submenus are created from this delegate.
        delegate: BsMenuItem {}

        BsMenuItem {
            text: qsTr("&New Workspace…")
            icon.source: "qrc:/images/svg/note_add.svg"
            onTriggered: appMenuBar.newWorkspaceRequested()
        }

        BsMenuItem {
            text: qsTr("&Open Workspace…")
            icon.source: "qrc:/images/svg/folder_open.svg"
            onTriggered: appMenuBar.openWorkspaceRequested()
        }

        Menu {
            id: recentMenu

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
            icon.source: "qrc:/images/svg/save.svg"
            enabled: appMenuBar.hasWorkspace && appMenuBar.workspaceDirty
            onTriggered: appMenuBar.saveWorkspaceRequested()
        }

        BsMenuItem {
            text: qsTr("Save &As…")
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
            enabled: false
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
            icon.source: "qrc:/images/svg/exit_to_app.svg"
            onTriggered: appMenuBar.quitRequested()
        }
    }

    Menu {
        title: qsTr("&Request")
        delegate: BsMenuItem {}

        BsMenuItem {
            text: qsTr("&New Request")
            icon.source: "qrc:/images/svg/note_add.svg"
            enabled: false
        }

        BsMenuItem {
            text: qsTr("New &Folder")
            icon.source: "qrc:/images/svg/folder.svg"
            enabled: false
        }

        MenuSeparator {}

        BsMenuItem {
            text: qsTr("&Send")
            icon.source: "qrc:/images/svg/play_arrow.svg"
            enabled: false
        }

        BsMenuItem {
            text: qsTr("&Cancel")
            icon.source: "qrc:/images/svg/stop.svg"
            enabled: false
        }

        MenuSeparator {}

        BsMenuItem {
            text: qsTr("&Duplicate")
            icon.source: "qrc:/images/svg/content_copy.svg"
            enabled: false
        }

        BsMenuItem {
            text: qsTr("Copy as c&URL")
            icon.source: "qrc:/images/svg/code.svg"
            enabled: false
        }
    }

    Menu {
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
