// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsTopBarActions
    \inqmlmodule Base
    \brief Right-aligned quick-action buttons for the top row.

    Sits next to the menu bar in the same top row and offers the most-used
    workspace actions. The host supplies the state through properties and wires
    the signals to the matching handlers; the component reads no application
    context property. The environment selector joins this row with the HTTP
    features of a later milestone.
*/
RowLayout {
    id: root

    /*! Whether the enclosing screen uses the dark theme. */
    property bool darkTheme: false

    /*! Whether the open workspace can be saved (open and with unsaved changes). */
    property bool canSave: false

    /*! Neutral tint used for inactive action icons. */
    readonly property color mutedColor: Qt.rgba(Material.foreground.r,
                                                Material.foreground.g,
                                                Material.foreground.b,
                                                0.4)

    /*! Emitted to save the open workspace. */
    signal saveWorkspaceRequested()

    /*! Emitted to open the application settings. */
    signal settingsRequested()

    spacing: 2

    // Save the open workspace.
    ToolButton {
        Layout.alignment: Qt.AlignVCenter
        display: AbstractButton.IconOnly
        icon.source: "qrc:/images/svg/save.svg"
        icon.width: 20
        icon.height: 20
        enabled: root.canSave
        icon.color: enabled ? Material.accent : root.mutedColor
        Accessible.name: qsTr("Save workspace")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Save workspace")
        onClicked: root.saveWorkspaceRequested()
    }

    // Open the application settings.
    ToolButton {
        Layout.alignment: Qt.AlignVCenter
        display: AbstractButton.IconOnly
        icon.source: "qrc:/images/svg/settings.svg"
        icon.width: 20
        icon.height: 20
        icon.color: Material.foreground
        Accessible.name: qsTr("Settings")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Settings")
        onClicked: root.settingsRequested()
    }

    // Trailing spacing so the last item is not flush against the window edge.
    Item {
        Layout.preferredWidth: 6
    }
}
