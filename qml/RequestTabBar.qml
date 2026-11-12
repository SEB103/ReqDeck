// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype RequestTabBar
    \inqmlmodule ReqDeck
    \brief The open request tabs above the request: click to switch, middle click or × to close.

    Uses \c cppTabs. The preview tab shows its name in italics; a double click
    keeps it.
*/
ListView {
    id: bar

    /*! Topic of the F1 help. */
    property string helpTopic: "request.openTabs"

    objectName: "requestTabBar"
    implicitHeight: 30
    orientation: ListView.Horizontal
    clip: true
    spacing: 1
    boundsBehavior: Flickable.StopAtBounds
    model: cppTabs
    currentIndex: cppTabs.currentIndex
    Accessible.name: qsTr("Open requests")

    ScrollBar.horizontal: ScrollBar {
        policy: ScrollBar.AsNeeded
        height: 4
    }

    delegate: ItemDelegate {
        id: tab

        required property int index
        required property string name
        required property string method
        required property bool preview
        required property bool busy

        objectName: "requestTab"
        width: Math.min(220, implicitContentWidth + leftPadding + rightPadding)
        height: bar.height
        leftPadding: 8
        rightPadding: 2
        highlighted: tab.index === cppTabs.currentIndex
        Accessible.role: Accessible.PageTab
        Accessible.name: tab.method + " " + tab.name + (tab.preview ? qsTr(", preview") : "")
        ToolTip.visible: hovered && tab.name.length > 24
        ToolTip.text: tab.name
        onClicked: cppTabs.currentIndex = tab.index
        onDoubleClicked: {
            cppTabs.currentIndex = tab.index
            cppTabs.keepCurrent()
        }

        contentItem: RowLayout {
            spacing: 6

            Label {
                font.pixelSize: 11
                font.bold: true
                text: tab.method
                color: Base.BsTheme.methodColor(tab.method)
            }

            Label {
                Layout.maximumWidth: 140
                elide: Text.ElideRight
                font.italic: tab.preview
                text: tab.name
            }

            BusyIndicator {
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                visible: tab.busy
                running: tab.busy
            }

            ToolButton {
                objectName: "closeTab"

                Layout.preferredWidth: 22
                Layout.preferredHeight: 22
                padding: 2
                icon.source: "qrc:/images/svg/close.svg"
                icon.width: 12
                icon.height: 12
                Accessible.name: qsTr("Close %1").arg(tab.name)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close (Ctrl+W)")
                onClicked: cppTabs.close(tab.index)
            }
        }

        // A mouse area below the content takes only the middle button, so the
        // left button stays with the delegate.
        MouseArea {
            anchors.fill: parent
            z: -1
            acceptedButtons: Qt.MiddleButton
            onClicked: cppTabs.close(tab.index)
        }
    }
}
