// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsNotificationBanner
    \inqmlmodule Base
    \brief Transient banner for outcomes the user must not miss.

    Only warnings and errors reach the banner; routine progress stays in the
    status bar so the workspace is not covered by messages that need no reaction.
    A warning dismisses itself after a few seconds, while an error waits for the
    user, because an error usually means an action did not happen.
*/
Rectangle {
    id: root

    /*! Text currently shown; empty hides the banner. */
    property string message: ""

    /*! Severity of \l message as a Diagnostics::Level value. */
    property int level: 2

    /*! Milliseconds a warning stays before it hides itself. */
    property int autoHideInterval: 8000

    // 3 is Diagnostics::Error.
    /*! Whether the current message waits for the user instead of hiding itself. */
    readonly property bool persistent: root.level === 3

    /*! Accent colour matching \l level. */
    readonly property color accentColor: root.persistent
                                         ? Material.color(Material.Red)
                                         : Material.color(Material.Amber)

    /*!
        Shows \a text at severity \a level, restarting the auto-hide timer.
        Levels below Warning are ignored so routine progress does not pop up.
    */
    function show(level, text) {
        if (level < 2 || !text || text.length === 0)
            return
        root.level = level
        root.message = text
        if (!root.persistent)
            hideTimer.restart()
        else
            hideTimer.stop()
    }

    /*! Hides the banner and clears the current message. */
    function dismiss() {
        hideTimer.stop()
        root.message = ""
    }

    visible: root.message.length > 0
    implicitHeight: visible ? 34 : 0
    color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.16)

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        color: root.accentColor
    }

    Timer {
        id: hideTimer

        interval: root.autoHideInterval
        onTriggered: root.message = ""
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 2
        spacing: 8

        Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: root.message
            font.pixelSize: 12
            elide: Text.ElideRight
            color: Material.foreground
        }

        ToolButton {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 26
            Layout.preferredHeight: 26
            text: "✕"
            font.pixelSize: 12
            Accessible.name: qsTr("Dismiss the message")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Dismiss")
            onClicked: root.dismiss()
        }
    }
}
