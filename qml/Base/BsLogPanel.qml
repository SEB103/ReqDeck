// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsLogPanel
    \inqmlmodule Base
    \brief Collapsible view of the application log.

    The panel shows the messages the application already produced, filtered by
    severity and free text, so a failed request or file operation can be inspected without
    leaving the application or opening the log file. It reads the filtered model
    published by the engine and never stores anything itself.
*/
Rectangle {
    id: root

    /*! Severity choices offered by the level selector, lowest first. */
    readonly property var levelChoices: [
        { label: qsTr("Debug"),   level: 0 },
        { label: qsTr("Info"),    level: 1 },
        { label: qsTr("Warning"), level: 2 },
        { label: qsTr("Error"),   level: 3 }
    ]

    /*! Whether the log follows new entries as they arrive. */
    property bool followTail: true

    /*! Emitted when the user closes the panel. */
    signal closeRequested()

    /*! Neutral tint used for secondary text and inactive icons. */
    readonly property color mutedColor: Qt.rgba(Material.foreground.r,
                                                Material.foreground.g,
                                                Material.foreground.b,
                                                0.45)

    /*! Returns the colour for the Diagnostics::Level \a level. */
    function levelColor(level) {
        if (level === 3)
            return Material.color(Material.Red)
        if (level === 2)
            return Material.color(Material.Amber)
        if (level === 0)
            return root.mutedColor
        return Material.foreground
    }

    color: Material.background
    border.color: BsTheme.dividerColor
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: BsTheme.headerColor

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 4
                spacing: 6

                Label {
                    text: qsTr("LOG")
                    font.pixelSize: 12
                    font.bold: true
                    font.letterSpacing: 1.2
                    color: BsTheme.accentTextColor
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    Layout.leftMargin: 4
                    text: qsTr("%n entries", "", cppAppEngine.logModel.count)
                    font.pixelSize: 12
                    color: root.mutedColor
                    verticalAlignment: Text.AlignVCenter
                }

                Item {
                    Layout.fillWidth: true
                }

                ComboBox {
                    id: levelBox

                    Layout.preferredWidth: 120
                    Layout.maximumHeight: 28
                    model: root.levelChoices
                    textRole: "label"
                    font.pixelSize: 12
                    // Info is the default so routine debug detail stays hidden.
                    currentIndex: 1
                    onActivated: cppAppEngine.logModel.minimumLevel =
                                 root.levelChoices[currentIndex].level
                }

                TextField {
                    id: filterField

                    Layout.preferredWidth: 180
                    Layout.maximumHeight: 28
                    placeholderText: qsTr("Filter messages")
                    font.pixelSize: 12
                    selectByMouse: true
                    onTextChanged: cppAppEngine.logModel.filterText = text
                }

                ToolButton {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/images/svg/save.svg"
                    icon.width: 15
                    icon.height: 15
                    icon.color: root.mutedColor
                    enabled: cppAppEngine.logModel.count > 0
                    Accessible.name: qsTr("Copy the shown log entries")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Copy the shown entries")
                    onClicked: cppAppEngine.copyToClipboard(
                                   cppAppEngine.logModel.visibleText())
                }

                ToolButton {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/images/svg/folder.svg"
                    icon.width: 15
                    icon.height: 15
                    icon.color: root.mutedColor
                    Accessible.name: qsTr("Open the log file location")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Open the folder holding the log file")
                    onClicked: cppAppEngine.showLogFileLocation()
                }

                ToolButton {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    text: "⌫"
                    font.pixelSize: 13
                    Accessible.name: qsTr("Clear the log")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Clear the log")
                    onClicked: cppAppEngine.clearLog()
                }

                ToolButton {
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    text: "✕"
                    font.pixelSize: 13
                    Accessible.name: qsTr("Close the log panel")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Hide the log panel")
                    onClicked: root.closeRequested()
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: BsTheme.dividerColor
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                visible: logView.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: filterField.text.length > 0
                      ? qsTr("No log entry matches the filter.")
                      : qsTr("No log entry at this severity yet.")
                color: Material.foreground
                opacity: 0.6
            }

            ListView {
                id: logView

                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: cppAppEngine.logModel
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true

                // Follow the tail only while the user has not scrolled away.
                onCountChanged: {
                    if (root.followTail)
                        positionViewAtEnd()
                }
                onMovementEnded: root.followTail = atYEnd

                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: logDelegate

                    required property string timestamp
                    required property string levelName
                    required property string category
                    required property string message
                    required property int level

                    width: logView.width
                    implicitHeight: logLabel.implicitHeight + 4

                    Label {
                        id: logLabel

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        text: logDelegate.category.length > 0
                              ? qsTr("%1  %2  %3: %4").arg(logDelegate.timestamp)
                                                      .arg(logDelegate.levelName)
                                                      .arg(logDelegate.category)
                                                      .arg(logDelegate.message)
                              : qsTr("%1  %2  %3").arg(logDelegate.timestamp)
                                                  .arg(logDelegate.levelName)
                                                  .arg(logDelegate.message)
                        font.family: "Consolas, monospace"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        color: root.levelColor(logDelegate.level)
                    }
                }
            }
        }
    }
}
