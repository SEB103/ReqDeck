// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsPane
    \inqmlmodule Base
    \brief Workspace pane with a titled header strip and a content area.

    Draws the shared pane chrome: a header strip with the upper-case \l title in
    the accent text color, a divider line, and a bordered content area. Child
    items placed inside the pane fill the content area. While the pane has no
    content, \l placeholderText is shown centered instead.
*/
Rectangle {
    id: root

    /*! Title shown in the header strip. */
    property string title: ""

    /*! Text shown centered while the content area holds no child items. */
    property string placeholderText: ""

    /*! Content items placed below the header strip. */
    default property alias content: contentArea.data

    color: Material.background
    border.color: BsTheme.dividerColor
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: BsTheme.headerColor

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                font.pixelSize: 12
                font.bold: true
                font.letterSpacing: 1.2
                font.capitalization: Font.AllUppercase
                color: BsTheme.accentTextColor
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: BsTheme.dividerColor
            }
        }

        Item {
            id: contentArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                visible: contentArea.children.length === 1 && root.placeholderText.length > 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: root.placeholderText
                color: Material.foreground
                opacity: 0.6
            }
        }
    }
}
