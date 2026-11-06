// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsMenuItem
    \inqmlmodule Base
    \brief Menu item that highlights with the shared accent tint.

    A Material MenuItem whose hover and keyboard highlight uses
    \l {BsTheme}{BsTheme.menuItemHighlightColor} instead of the neutral Material
    list highlight, so dropdown and context menus match the menu bar. Size,
    padding, icons, check indicators, and submenu arrows are unchanged. Menus that
    contain submenus also set it as their \c delegate, because the entry of a
    submenu is created from the parent menu's delegate. The item names its
    shortcut through \l shortcutText; the shortcut itself lives elsewhere, because
    a disabled or closed menu would not trigger it.
*/
MenuItem {
    id: control

    /*! Shortcut shown at the end of the item, for example "Ctrl+S". */
    property string shortcutText: ""

    rightPadding: control.shortcutText.length > 0 ? shortcutLabel.implicitWidth + 40 : 16

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: control.Material.menuItemHeight
        color: control.highlighted ? BsTheme.menuItemHighlightColor : "transparent"
    }

    Label {
        id: shortcutLabel

        objectName: "shortcutLabel"

        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        visible: control.shortcutText.length > 0
        opacity: 0.6
        text: control.shortcutText
    }
}
