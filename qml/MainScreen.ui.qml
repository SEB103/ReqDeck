// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype MainScreen
    \inqmlmodule ReqDeck
    \brief Composes the menu band, the three-pane workspace, and the log panel.

    The workspace is split into the sidebar (collection tree and history), the
    request editor, and the response viewer. The panes show placeholders until
    the HTTP features fill them. Main.qml supplies the state and connects to the
    exposed menu bar, quick actions, banner, and log panel.
*/
Pane {
    id: main

    /*! Exposes the menu bar so Main can connect to its public signals. */
    property alias menuBar: menuBar

    /*! Exposes the top-bar quick actions so Main can connect to their signals. */
    property alias topActions: topActions

    /*! Exposes the notification banner so Main can feed it. */
    property alias notificationBanner: notificationBanner

    /*! Exposes the log panel so Main can react to its close request. */
    property alias logPanel: logPanel

    /*! Whether child controls should follow the dark theme state. */
    property bool darkTheme: false

    /*! Whether the collapsible log panel is shown below the workspace. */
    property bool logPanelVisible: false

    /*! Height of the log panel while it is shown. */
    property int logPanelHeight: 200

    /*! Product name shown in the Help menu. */
    property string appName: ""

    /*! Whether a workspace is open. */
    property bool hasWorkspace: false

    /*! Whether the open workspace has unsaved changes. */
    property bool workspaceDirty: false

    /*! Recent-workspace rows forwarded to the menu bar. */
    property var recentWorkspaces: []

    width: 1200
    height: 800
    padding: 0

    // A column keeps the workspace, the banner, and the log panel from
    // overlapping: a hidden layout child takes no space.
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Single top row: menu titles on the left, quick actions on the right,
        // sharing one tinted band that ends in a divider line.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: topBar.implicitHeight + 1
            color: Base.BsTheme.menuBandColor

            RowLayout {
                id: topBar

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: 0

                Base.BsMenuBar {
                    id: menuBar

                    Layout.fillWidth: true
                    darkTheme: main.darkTheme
                    logPanelVisible: main.logPanelVisible
                    appName: main.appName
                    hasWorkspace: main.hasWorkspace
                    workspaceDirty: main.workspaceDirty
                    recentWorkspaces: main.recentWorkspaces
                }

                Base.BsTopBarActions {
                    id: topActions

                    Layout.alignment: Qt.AlignVCenter
                    darkTheme: main.darkTheme
                    canSave: main.hasWorkspace && main.workspaceDirty
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Base.BsTheme.dividerColor
            }
        }

        Base.BsNotificationBanner {
            id: notificationBanner

            Layout.fillWidth: true
        }

        SplitView {
            id: workspaceSplit

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 8
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 8
                color: "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: SplitHandle.pressed ? 3 : 2
                    height: parent.height
                    radius: 1
                    color: SplitHandle.pressed
                           ? Material.accent
                           : SplitHandle.hovered
                             ? Base.BsTheme.splitHandleHoverColor
                             : Base.BsTheme.dividerColor
                }
            }

            Base.BsPane {
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 200
                SplitView.fillHeight: true
                title: qsTr("Collection")
                placeholderText: qsTr("Requests and history appear here.")
            }

            Base.BsPane {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 300
                SplitView.fillHeight: true
                title: qsTr("Request")
                placeholderText: qsTr("Select or create a request to edit it here.")
            }

            Base.BsPane {
                SplitView.preferredWidth: 420
                SplitView.minimumWidth: 260
                SplitView.fillHeight: true
                title: qsTr("Response")
                placeholderText: qsTr("Send a request to see the response here.")
            }
        }

        Base.BsLogPanel {
            id: logPanel

            Layout.fillWidth: true
            Layout.preferredHeight: main.logPanelHeight
            Layout.minimumHeight: 100
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.bottomMargin: 8
            visible: main.logPanelVisible
        }
    }
}
