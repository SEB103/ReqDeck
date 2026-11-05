// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

/*!
    \qmltype BsStatusChip
    \inqmlmodule Base
    \brief Shows an HTTP status in the color of its class.

    Status colors are content, not chrome, so they are defined here and not in
    BsTheme; only the choice between the light and dark variant follows the theme.
*/
Rectangle {
    id: root

    /*! First digit of the HTTP status code; 0 when there is none. */
    property int statusClass: 0

    /*! Text of the chip, for example "200 OK". */
    property string text: ""

    /*! Color of the status class in the active theme. */
    readonly property color classColor: {
        switch (root.statusClass) {
        case 2:
            return BsTheme.dark ? "#81C784" : "#2E7D32"
        case 3:
            return BsTheme.dark ? "#64B5F6" : "#1565C0"
        case 4:
            return BsTheme.dark ? "#FFB74D" : "#B85C00"
        case 5:
            return BsTheme.dark ? "#E57373" : "#C62828"
        default:
            return BsTheme.dark ? "#BDBDBD" : "#616161"
        }
    }

    implicitWidth: label.implicitWidth + 16
    implicitHeight: label.implicitHeight + 6
    radius: 4
    color: Qt.alpha(root.classColor, 0.16)

    Label {
        id: label

        objectName: "label"

        anchors.centerIn: parent
        text: root.text
        font.bold: true
        color: root.classColor
    }
}
