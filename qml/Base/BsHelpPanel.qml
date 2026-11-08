// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsHelpPanel
    \inqmlmodule Base
    \brief Short explanation of one element, shown at the right edge.

    The panel does not take the focus and does not block the window, so the user
    can go on working while reading it; it stays above dialogs. The host fills
    \l topic and \l labels and opens or closes it. All text is plain.
*/
Popup {
    id: root

    /*! Shown topic with title, what, why, example, next, and chapter. */
    property var topic: ({})

    /*! Headings in the language of the topic: why, example, next, more, close. */
    property var labels: ({})

    /*! Emitted when the user asks for the guide chapter \a chapter. */
    signal moreRequested(string chapter)

    /*! Returns the text of \a key in \a map, or an empty string. */
    function textOf(map, key) {
        return map && map[key] !== undefined ? String(map[key]) : ""
    }

    /*! One headed part of a topic, shown only when the topic has it. */
    component HelpSection: ColumnLayout {
        id: section

        /*! Part of the topic and of the headings, for example "why". */
        property string key: ""

        Layout.fillWidth: true
        spacing: 2
        visible: root.textOf(root.topic, section.key).length > 0

        Label {
            Layout.fillWidth: true
            textFormat: Text.PlainText
            font.pixelSize: 11
            font.bold: true
            font.capitalization: Font.AllUppercase
            color: BsTheme.accentTextColor
            text: root.textOf(root.labels, section.key)
        }

        Label {
            objectName: "help_" + section.key

            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            font.family: section.key === "example" ? "Consolas" : Qt.application.font.family
            text: root.textOf(root.topic, section.key)
        }
    }

    objectName: "helpPanel"

    parent: Overlay.overlay
    x: parent ? parent.width - width - 12 : 0
    y: 52
    z: 1000
    width: 340
    padding: 16
    modal: false
    dim: false
    focus: false
    closePolicy: Popup.NoAutoClose
    Material.roundedScale: Material.SmallScale
    Material.elevation: 6

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: 120
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                objectName: "helpTitle"

                Layout.fillWidth: true
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                font.pixelSize: 16
                font.bold: true
                text: root.textOf(root.topic, "title")
            }

            ToolButton {
                objectName: "helpClose"

                display: AbstractButton.IconOnly
                icon.source: "qrc:/images/svg/close.svg"
                icon.width: 16
                icon.height: 16
                focusPolicy: Qt.NoFocus
                Accessible.name: root.textOf(root.labels, "close")
                onClicked: root.close()
            }
        }

        Label {
            objectName: "helpWhat"

            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: root.textOf(root.topic, "what")
        }

        HelpSection {
            key: "why"
        }

        HelpSection {
            key: "example"
        }

        HelpSection {
            key: "next"
        }

        Label {
            objectName: "helpChapter"

            Layout.fillWidth: true
            visible: root.textOf(root.topic, "chapterTitle").length > 0
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            opacity: 0.7
            text: root.textOf(root.labels, "chapter") + ": " + root.textOf(root.topic, "chapterTitle")
        }

        Button {
            objectName: "helpMore"

            flat: true
            focusPolicy: Qt.NoFocus
            visible: root.textOf(root.topic, "chapter").length > 0
            text: root.textOf(root.labels, "more")
            onClicked: root.moreRequested(root.textOf(root.topic, "chapter"))
        }
    }
}
