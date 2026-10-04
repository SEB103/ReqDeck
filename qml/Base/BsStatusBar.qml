// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsStatusBar
    \inqmlmodule Base
    \brief Permanent footer showing the open workspace, workload, and the last outcome.

    The bar answers what is open, whether an operation is running, and how the
    last operation ended. All values are supplied by the window, which collects
    them from the application facades, so this component stays a pure display and
    reads no application context property.
*/
Rectangle {
    id: root

    /*! Display name of the open workspace; empty when none is open. */
    property string workspaceName: ""

    /*! Whether the open workspace has unsaved changes. */
    property bool workspaceDirty: false

    /*! Whether an operation is running; shows the progress indicator and tint. */
    property bool busy: false

    /*! Text of the most recent outcome; empty leaves the message area blank. */
    property string message: ""

    /*! Severity of \l message as a Diagnostics::Level value. */
    property int messageLevel: 1

    /*! Whether the log panel is currently shown, used to highlight the toggle. */
    property bool logPanelVisible: false

    /*! Neutral tint used for secondary text. */
    readonly property color mutedColor: Qt.rgba(Material.foreground.r,
                                                Material.foreground.g,
                                                Material.foreground.b,
                                                0.55)

    /*! Colour matching \l messageLevel. */
    readonly property color messageColor: {
        // 2 Warning, 3 Error in Diagnostics::Level.
        if (root.messageLevel === 3)
            return Material.color(Material.Red)
        if (root.messageLevel === 2)
            return Material.color(Material.Amber)
        return root.mutedColor
    }

    /*! Emitted when the user asks to show or hide the log panel. */
    signal logToggleRequested()

    implicitHeight: 28
    color: BsTheme.statusBarColor(root.busy)

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: BsTheme.statusLineWidth(root.busy)
        color: BsTheme.statusLineColor(root.busy)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 2
        spacing: 8

        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.maximumWidth: 320
            text: root.workspaceName.length > 0 ? root.workspaceName : qsTr("No workspace")
            font.pixelSize: 12
            font.weight: root.workspaceName.length > 0 ? Font.DemiBold : Font.Normal
            elide: Text.ElideMiddle
            color: root.workspaceName.length > 0 ? BsTheme.accentText : root.mutedColor
        }

        Label {
            Layout.alignment: Qt.AlignVCenter
            visible: root.workspaceName.length > 0 && root.workspaceDirty
            text: qsTr("Unsaved changes")
            font.pixelSize: 12
            color: root.mutedColor
        }

        // Progress indication for running operations.
        ProgressBar {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 90
            visible: root.busy
            indeterminate: true
        }

        Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: root.message
            font.pixelSize: 12
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            color: root.messageColor
            ToolTip.visible: messageHover.hovered && root.message.length > 0
            ToolTip.text: root.message

            HoverHandler {
                id: messageHover
            }
        }

        ToolButton {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 26
            Layout.preferredHeight: 26
            display: AbstractButton.IconOnly
            icon.source: "qrc:/images/svg/info.svg"
            icon.width: 15
            icon.height: 15
            icon.color: root.logPanelVisible ? Material.accent : root.mutedColor
            Accessible.name: qsTr("Toggle the log panel")
            ToolTip.visible: hovered
            ToolTip.text: root.logPanelVisible ? qsTr("Hide the log panel")
                                               : qsTr("Show the log panel")
            onClicked: root.logToggleRequested()
        }
    }
}
