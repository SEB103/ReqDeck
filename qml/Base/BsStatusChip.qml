// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

/*!
    \qmltype BsStatusChip
    \inqmlmodule Base
    \brief Shows an HTTP status in the color of its class.

    The colors come from BsTheme.statusColor(), so that the chip and the history
    show a status alike. A chip without a status class is grey.
*/
Rectangle {
    id: root

    /*! First digit of the HTTP status code; 0 when there is none. */
    property int statusClass: 0

    /*! Text of the chip, for example "200 OK". */
    property string text: ""

    /*! Color of the status class in the active theme. */
    readonly property color classColor: root.statusClass >= 2 && root.statusClass <= 5
        ? BsTheme.statusColor(root.statusClass * 100)
        : (BsTheme.dark ? "#BDBDBD" : "#616161")

    implicitWidth: label.implicitWidth + 16
    implicitHeight: label.implicitHeight + 6
    radius: 4
    color: Qt.alpha(root.classColor, 0.16)

    Label {
        id: label

        objectName: "label"

        anchors.centerIn: parent
        width: Math.min(implicitWidth, root.width - 16)
        elide: Text.ElideRight
        textFormat: Text.PlainText
        text: root.text
        font.bold: true
        color: root.classColor
    }
}
