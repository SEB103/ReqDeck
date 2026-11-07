// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsInlineTitle
    \inqmlmodule Base
    \brief Editable title without an outline.

    Reads as a heading and shows that it can be edited on hover and focus. A text
    longer than the control is clipped and shows its start while it is not being
    edited. There is no placeholder, because a title always has text; set
    \c {Accessible.name} instead.
*/
TextField {
    id: root

    /*! Shows the start of a long title, unless the user is editing it. */
    function showStart() {
        if (!root.activeFocus)
            root.cursorPosition = 0
    }

    implicitHeight: 30
    leftPadding: 4
    rightPadding: 4
    topPadding: 2
    bottomPadding: 2
    font.pixelSize: 15
    font.weight: Font.DemiBold
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    onTextChanged: root.showStart()
    onActiveFocusChanged: root.showStart()

    background: Rectangle {
        radius: 4
        color: root.hovered || root.activeFocus ? Qt.alpha(BsTheme.accent, 0.06) : "transparent"

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            visible: root.activeFocus
            color: BsTheme.accent
        }
    }
}
