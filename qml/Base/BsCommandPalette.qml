// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsCommandPalette
    \inqmlmodule Base
    \brief Finds a request or a command by typing a few words of it.

    The host fills \l entries and runs what \l chosen() names. An entry matches
    when its title and detail contain every typed word, in any order and without
    regard to case. Arrows move, Return or Enter chooses, Escape closes.
*/
Popup {
    id: palette

    /*! Topic of the F1 help. */
    property string helpTopic: "palette"

    /*! Entries to offer: objects with \c key, \c title, and \c detail. */
    property var entries: []

    /*! Entries that match the typed words, in the order of \l entries. */
    readonly property var matches: palette.filter(palette.entries, searchField.text)

    /*! Index of the highlighted match. */
    property int current: 0

    /*! Emitted with the \a key of the entry the user chose. */
    signal chosen(string key)

    /*! Returns the entries of \a list whose title and detail contain every word of \a text. */
    function filter(list, text) {
        const words = text.toLowerCase().split(/\s+/).filter(word => word.length > 0)
        return list.filter(entry => {
            const haystack = (entry.title + " " + (entry.detail || "")).toLowerCase()
            return words.every(word => haystack.indexOf(word) >= 0)
        })
    }

    /*! Clears the search and opens the palette. */
    function show() {
        searchField.text = ""
        palette.current = 0
        palette.open()
        searchField.forceActiveFocus()
    }

    /*! Chooses the match at \a index and closes the palette. */
    function choose(index) {
        if (index < 0 || index >= palette.matches.length)
            return
        const key = palette.matches[index].key
        palette.close()
        palette.chosen(key)
    }

    objectName: "commandPalette"
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: 60
    width: 520
    padding: 8
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: searchField.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: 6

        TextField {
            id: searchField

            objectName: "paletteSearch"
            Layout.fillWidth: true
            placeholderText: qsTr("Type a request or a command")
            selectByMouse: true
            Accessible.name: qsTr("Search requests and commands")
            onTextChanged: palette.current = 0
            Keys.onPressed: (event) => {
                const count = palette.matches.length
                if (event.key === Qt.Key_Down && count > 0) {
                    palette.current = (palette.current + 1) % count
                } else if (event.key === Qt.Key_Up && count > 0) {
                    palette.current = (palette.current - 1 + count) % count
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    palette.choose(palette.current)
                } else {
                    return
                }
                event.accepted = true
            }
        }

        ListView {
            id: matchList

            objectName: "paletteList"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 320)
            clip: true
            model: palette.matches
            currentIndex: palette.current
            onCurrentIndexChanged: matchList.positionViewAtIndex(matchList.currentIndex, ListView.Contain)

            delegate: ItemDelegate {
                required property int index
                required property var modelData

                width: ListView.view.width
                highlighted: index === palette.current
                onClicked: palette.choose(index)

                contentItem: RowLayout {
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        text: modelData.title
                    }

                    Label {
                        Layout.maximumWidth: 260
                        textFormat: Text.PlainText
                        elide: Text.ElideMiddle
                        opacity: 0.6
                        font.pixelSize: 11
                        text: modelData.detail || ""
                    }
                }
            }
        }

        Label {
            objectName: "paletteEmpty"

            visible: palette.matches.length === 0
            opacity: 0.6
            text: qsTr("Nothing matches.")
        }
    }
}
