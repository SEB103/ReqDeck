// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsVariableCompleter
    \inqmlmodule Base
    \brief Offers variable names after "{{" in a text field.

    The host field calls refresh() after each edit and passes its key presses to
    handleKey() first. On a choice the completer emits completed() with the new
    text and cursor position, which the host applies and stores. Only a plain
    Return or Enter is taken, so Ctrl+Enter still reaches the Send shortcut.
*/
Popup {
    id: completer

    /*! Topic of the F1 help. */
    property string helpTopic: "completer"

    /*! Field to complete; it needs \c text and \c cursorPosition. */
    property Item target: null

    /*! Names that can be offered. */
    property var names: []

    /*! Names that match what was typed after "{{". */
    property var matches: []

    /*! Position of the "{{" being completed, or -1. */
    property int start: -1

    /*! Index of the highlighted name. */
    property int current: 0

    /*! Emitted with the completed \a text and the \a cursorPosition after the inserted name. */
    signal completed(string text, int cursorPosition)

    /*! Looks at the text before the cursor and opens with the matching names, or closes. */
    function refresh() {
        if (!completer.target) {
            completer.close()
            return
        }
        const before = completer.target.text.slice(0, completer.target.cursorPosition)
        const open = before.lastIndexOf("{{")
        const prefix = open < 0 ? "" : before.slice(open + 2).replace(/^\s+/, "")
        if (open < 0 || before.indexOf("}}", open) >= 0 || !/^[A-Za-z0-9_.\-]*$/.test(prefix)) {
            completer.close()
            return
        }
        const lower = prefix.toLowerCase()
        completer.matches = completer.names.filter(name => name.toLowerCase().startsWith(lower)
                                                            && name !== prefix)
        completer.start = open
        completer.current = 0
        if (completer.matches.length > 0)
            completer.open()
        else
            completer.close()
    }

    /*! Inserts the name at \a index with its closing braces and emits completed(). */
    function choose(index) {
        if (index < 0 || index >= completer.matches.length || !completer.target)
            return
        const text = completer.target.text
        // The cursor may have moved since the list opened: insert only where it
        // still follows the "{{" being completed.
        const typed = text.slice(completer.start, completer.target.cursorPosition)
        if (completer.start < 0 || !/^\{\{\s*[A-Za-z0-9_.\-]*$/.test(typed)) {
            completer.close()
            return
        }
        const name = completer.matches[index]
        let after = text.slice(completer.target.cursorPosition)
        // Braces the user already typed are not doubled.
        if (/^\s*\}\}/.test(after))
            after = after.slice(after.indexOf("}}") + 2)
        const result = text.slice(0, completer.start) + "{{" + name + "}}" + after
        const cursor = completer.start + name.length + 4
        completer.close()
        completer.completed(result, cursor)
    }

    /*!
        Handles \a event while the list is open: arrows move, Return, Enter, and
        Tab choose, Escape closes. Returns whether the key was taken.
    */
    function handleKey(event) {
        if (!completer.visible)
            return false
        if (event.key === Qt.Key_Down || event.key === Qt.Key_Up) {
            const step = event.key === Qt.Key_Down ? 1 : -1
            completer.current = (completer.current + step + completer.matches.length) % completer.matches.length
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Tab)
                   && event.modifiers === Qt.NoModifier) {
            completer.choose(completer.current)
        } else if (event.key === Qt.Key_Escape) {
            completer.close()
        } else {
            return false
        }
        event.accepted = true
        return true
    }

    parent: completer.target
    // Below the cursor in a text field or area; below the whole control otherwise.
    x: completer.target && completer.target.cursorRectangle !== undefined
       ? Math.max(0, Math.min(completer.target.cursorRectangle.x, completer.target.width - completer.width)) : 0
    y: !completer.target ? 0
       : completer.target.cursorRectangle !== undefined
         ? completer.target.cursorRectangle.y + completer.target.cursorRectangle.height + 2
         : completer.target.height
    width: 260
    padding: 2
    focus: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    // A cursor that moves away, by a key or a click, ends the completion.
    Connections {
        target: completer.target
        enabled: completer.visible

        function onCursorPositionChanged() {
            completer.refresh()
        }
    }

    contentItem: ListView {
        objectName: "completerList"

        implicitHeight: Math.min(contentHeight, 220)
        clip: true
        model: completer.matches
        currentIndex: completer.current

        delegate: ItemDelegate {
            required property int index
            required property string modelData

            width: ListView.view.width
            text: "{{" + modelData + "}}"
            font.family: "Consolas"
            highlighted: index === completer.current
            onClicked: completer.choose(index)
        }
    }
}
