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

    The workspace is split into the collection sidebar, the request editor, and
    the response viewer. Main.qml
    supplies the state and connects to the exposed menu bar, quick actions,
    banner, log panel, and collection pane.
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

    /*! Exposes the collection pane so Main can connect it to the menu and editor. */
    property alias collectionPane: collectionPane

    /*! Exposes the history pane so Main can show requests made from it. */
    property alias historyPane: historyPane

    /*! The request editor pane. */
    property alias requestPane: requestPane

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

    /*! Whether a copy of the workspace from before a format change exists. */
    property bool hasPreviousCopy: false

    /*! Names of the environments forwarded to the top bar. */
    property var environmentNames: []

    /*! Row of the active environment forwarded to the top bar. */
    property int activeEnvironmentIndex: -1

    /*! Recent-workspace rows forwarded to the menu bar. */
    property var recentWorkspaces: []

    /*! Whether a request is open in the editor. */
    property bool hasRequest: false

    /*! Whether the open request can be sent. */
    property bool canSend: false

    /*! Whether a running request can be cancelled. */
    property bool canCancel: false

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
                    hasPreviousCopy: main.hasPreviousCopy
                    recentWorkspaces: main.recentWorkspaces
                    hasSelection: collectionPane.selectedId.length > 0
                    hasRequest: main.hasRequest
                    canSend: main.canSend
                    canCancel: main.canCancel
                }

                Base.BsTopBarActions {
                    id: topActions

                    Layout.alignment: Qt.AlignVCenter
                    darkTheme: main.darkTheme
                    canSave: main.hasWorkspace && main.workspaceDirty
                    environmentNames: main.environmentNames
                    activeEnvironmentIndex: main.activeEnvironmentIndex
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

            /*! Topic of the F1 help. */
            property string helpTopic: "notification"

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
                title: qsTr("Workspace")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    TabBar {
                        id: sidebarTabs

                        /*! Topic of the F1 help. */
                        property string helpTopic: "sidebar.tabs"

                        objectName: "sidebarTabs"

                        Layout.fillWidth: true
                        contentHeight: Base.BsTheme.compactBarHeight

                        Base.BsTabButton {
                            text: qsTr("Collection")
                        }

                        Base.BsTabButton {
                            text: qsTr("History")
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: sidebarTabs.currentIndex

                        CollectionPane {
                            id: collectionPane

                            /*! Topic of the F1 help. */
                            property string helpTopic: "collection.tree"

                        }

                        HistoryPane {
                            id: historyPane

                            /*! Topic of the F1 help. */
                            property string helpTopic: "history.list"

                        }
                    }
                }
            }

            Base.BsPane {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 300
                SplitView.fillHeight: true
                title: qsTr("Request")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RequestTabBar {
                        Layout.fillWidth: true
                        visible: count > 0 && cppTabs.showTabs
                    }

                    RequestPane {
                        id: requestPane

                        objectName: "requestPane"

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                    }
                }
            }

            Base.BsPane {
                SplitView.preferredWidth: 420
                SplitView.minimumWidth: 260
                SplitView.fillHeight: true
                title: qsTr("Response")

                ResponsePane {
                    objectName: "responsePane"

                    anchors.fill: parent
                }
            }
        }

        Base.BsLogPanel {
            id: logPanel

            /*! Topic of the F1 help. */
            property string helpTopic: "log.panel"

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
