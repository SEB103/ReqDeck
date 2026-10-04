// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype LauncherScreen
    \inqmlmodule ReqDeck
    \brief Workspace selection start page shown when no workspace is open.

    Lists recent workspaces and offers opening an existing workspace or creating a
    new one. File selection is delegated to the host (Main) via
    \c openWorkspaceRequested and \c createWorkspaceRequested, while recent entries
    act directly on \c cppWorkspaceManager.
*/
Pane {
    id: launcher

    /*! Whether child controls should follow the dark theme state. */
    property bool darkTheme: false

    /*!
        \qmlsignal LauncherScreen::openWorkspaceRequested()
        Emitted when the user asks to open an existing workspace file. The host
        shows a file dialog and calls \c cppWorkspaceManager.openWorkspace().
    */
    signal openWorkspaceRequested()

    /*!
        \qmlsignal LauncherScreen::createWorkspaceRequested()
        Emitted when the user asks to create a new workspace. The host shows the
        create-workspace form and calls \c cppWorkspaceManager.createWorkspace().
    */
    signal createWorkspaceRequested()

    /*!
        Formats an ISO 8601 \a iso timestamp as a compact local date-time, or
        returns it unchanged when it cannot be parsed.
    */
    function formatTimestamp(iso) {
        if (!iso)
            return ""
        const date = new Date(iso)
        if (isNaN(date.getTime()))
            return iso
        return Qt.formatDateTime(date, "yyyy-MM-dd hh:mm")
    }

    padding: 0

    background: Rectangle {
        color: Material.background
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: card.implicitHeight + 80
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: card

            width: Math.min(parent.width - 80, 760)
            anchors.horizontalCenter: parent.horizontalCenter
            y: 40
            spacing: 16

            // Language selector shown before any workspace is opened, so the start
            // page itself already appears in the chosen language. Applies live. It
            // appears once more than one UI language ships.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: cppLocale.availableLanguages.length > 1

                Item { Layout.fillWidth: true }

                Label {
                    text: qsTr("Language:")
                    color: Material.foreground
                    opacity: 0.7
                }

                Base.BsLanguageSelector {
                    id: launcherLanguageCombo
                }
            }

            // Application logo shown at the top of the start page. The source is a
            // transparent-background PNG so it blends with either theme;
            // sourceSize caps the decoded size for a 128 px display.
            Image {
                source: "qrc:/images/app/ReqDeckLogo.png"
                fillMode: Image.PreserveAspectFit
                sourceSize.width: 256
                sourceSize.height: 256
                Layout.preferredWidth: 128
                Layout.preferredHeight: 128
                Accessible.role: Accessible.Graphic
                Accessible.name: cppAppInfo.appName
            }

            Label {
                text: cppAppInfo.appName
                font.pixelSize: 28
                font.bold: true
                color: Material.foreground
            }

            Label {
                text: qsTr("Select a workspace to begin. A workspace holds your request collections and environments.")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                color: Material.foreground
                opacity: 0.7
            }

            // Section header, matching the workspace pane style.
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.preferredHeight: 34
                color: Base.BsTheme.headerColor

                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("RECENT WORKSPACES")
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1.2
                    color: Base.BsTheme.accentTextColor
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Base.BsTheme.dividerColor
                }
            }

            Label {
                visible: cppWorkspaceManager.recentWorkspaces.length === 0
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                text: qsTr("No recent workspaces. Open an existing workspace or create a new one.")
                wrapMode: Text.Wrap
                color: Material.foreground
                opacity: 0.6
            }

            // Recent-workspace list. Each row opens the workspace on click; a
            // missing file is dimmed and can only be removed from the list.
            Repeater {
                model: cppWorkspaceManager.recentWorkspaces

                delegate: ItemDelegate {
                    id: recentRow

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    implicitHeight: 64
                    enabled: recentRow.modelData.available

                    onClicked: cppWorkspaceManager.openRecent(recentRow.index)

                    contentItem: RowLayout {
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: recentRow.modelData.displayName.length > 0
                                      ? recentRow.modelData.displayName
                                      : recentRow.modelData.path
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                color: Material.foreground
                            }

                            Label {
                                text: recentRow.modelData.available
                                      ? recentRow.modelData.path
                                      : qsTr("Unavailable — %1").arg(recentRow.modelData.path)
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                                color: recentRow.modelData.available ? Material.accent
                                                                     : Material.color(Material.Red)
                                opacity: recentRow.modelData.available ? 0.9 : 1.0
                            }
                        }

                        Label {
                            text: launcher.formatTimestamp(recentRow.modelData.lastOpened)
                            font.pixelSize: 12
                            color: Material.foreground
                            opacity: 0.6
                        }

                        ToolButton {
                            text: "✕"
                            flat: true
                            ToolTip.text: qsTr("Remove from list")
                            ToolTip.visible: hovered
                            onClicked: cppWorkspaceManager.removeRecent(recentRow.index)
                        }
                    }
                }
            }

            RowLayout {
                Layout.topMargin: 16
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Open Workspace…")
                    onClicked: launcher.openWorkspaceRequested()
                }

                Button {
                    text: qsTr("Create New Workspace")
                    highlighted: true
                    onClicked: launcher.createWorkspaceRequested()
                }
            }
        }
    }
}
