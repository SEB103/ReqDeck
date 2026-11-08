// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Material

/*!
    \qmltype BsUnknownVariablesMark
    \inqmlmodule Base
    \brief Frames the field it fills when its text references variables that do not resolve.

    Text inputs cannot color single characters, so the whole field is framed and
    its tooltip names the variables. The field shows the tooltip itself; see
    \l tooltip.
*/
Rectangle {
    id: mark

    /*! Names of the variables that do not resolve; empty hides the frame. */
    property var names: []

    /*! Text for the tooltip of the field; empty when nothing is unknown. */
    readonly property string tooltip: mark.names.length > 0
                                      ? qsTr("Unknown variable: %1").arg(mark.names.map(name => "{{" + name + "}}").join(", "))
                                      : ""

    objectName: "unknownMark"
    anchors.fill: parent
    anchors.margins: 1
    visible: mark.names.length > 0
    color: "transparent"
    radius: 3
    border.width: 1
    border.color: Material.color(Material.Red)
}
