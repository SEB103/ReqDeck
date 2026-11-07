// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsSegmentedControl
    \inqmlmodule Base
    \brief Small choice between labeled options.

    The host owns the choice: it binds \l currentIndex and updates it from
    \l activated. Each option takes the keyboard focus through Tab, and Space
    chooses it.
*/
Row {
    id: root

    /*! Labels of the options. */
    property var options: []

    /*! Row of the chosen option. */
    property int currentIndex: 0

    /*! Emitted when the user chooses the option at \a index. */
    signal activated(int index)

    spacing: 0

    Repeater {
        model: root.options

        delegate: AbstractButton {
            id: option

            required property string modelData
            required property int index
            readonly property bool chosen: root.currentIndex === option.index

            objectName: "option" + option.index

            implicitWidth: 58
            implicitHeight: 24
            focusPolicy: Qt.TabFocus
            text: option.modelData
            Accessible.name: option.modelData
            onClicked: root.activated(option.index)

            background: Rectangle {
                radius: 4
                color: option.chosen ? BsTheme.selectedRowColor : "transparent"
                border.width: option.activeFocus ? 2 : 1
                border.color: option.activeFocus ? BsTheme.accent : BsTheme.dividerColor
            }

            contentItem: Label {
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 12
                color: option.chosen ? BsTheme.accentTextColor : Material.foreground
                text: option.text
            }
        }
    }
}
