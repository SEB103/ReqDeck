// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsSingleLineEdit
    \inqmlmodule Base
    \brief One line of text with a text document for a syntax highlighter.

    TextField has no text document, and TextArea is made for several lines. This
    control keeps a TextArea to one line: Return adds nothing, Tab moves the focus,
    line breaks from a paste are removed, and the view scrolls with the cursor.

    The host sets \l text and writes back from \l textEdited. Showing a text never
    emits \l textEdited, not even when TextArea changes its line endings, so a text
    from a file that holds a line break stays as it is until the user edits it. A
    syntax highlighter recoloring the document makes TextArea report a text change
    too, so a change counts as an edit only when the text really differs.
*/
FocusScope {
    id: root

    /*! The text to show. */
    property string text: ""

    /*! Text shown while the control is empty. */
    property alias placeholderText: area.placeholderText

    /*! Font of the text. */
    property alias font: area.font

    /*! Position of the cursor. */
    property alias cursorPosition: area.cursorPosition

    /*! Number of characters shown. */
    readonly property alias length: area.length

    /*! Document of the text, for a syntax highlighter. */
    readonly property alias textDocument: area.textDocument

    /*!
        Emitted with the new text after the user changed it.
        \a edited is the text without line breaks.
    */
    signal textEdited(string edited)

    /*! Shows \l text unless it is already shown. */
    function showText() {
        if (root.text === area.shownText)
            return
        area.shownText = root.text
        area.showingText = true
        area.lineBreaksPending = false
        area.text = root.text
        area.showingText = false
        area.documentText = area.text
        flickable.contentX = 0
    }

    /*!
        Removes line breaks from the text and reports it.
        Runs after the change that brought them, which the text document must
        finish first.
    */
    function removeLineBreaks() {
        if (!area.lineBreaksPending)
            return
        area.lineBreaksPending = false
        const text = area.text
        const before = text.slice(0, area.cursorPosition).replace(/[\r\n]/g, "")
        const clean = text.replace(/[\r\n]/g, "")
        area.showingText = true
        area.text = clean
        area.showingText = false
        area.documentText = area.text
        area.cursorPosition = before.length
        area.shownText = clean
        root.textEdited(clean)
    }

    /*! Scrolls just far enough to show the cursor. */
    function followCursor() {
        const left = area.cursorRectangle.x - area.leftPadding
        const right = area.cursorRectangle.x + area.cursorRectangle.width + area.rightPadding
        if (left < flickable.contentX)
            flickable.contentX = Math.max(0, left)
        else if (right > flickable.contentX + flickable.width)
            flickable.contentX = right - flickable.width
    }

    implicitWidth: 200
    implicitHeight: area.implicitHeight - area.topInset
    onTextChanged: root.showText()
    Component.onCompleted: root.showText()

    Flickable {
        id: flickable

        // Material insets a clipped TextArea to keep its floating label inside. The
        // outline stays level with the neighbors, and the label takes the space above.
        anchors.fill: parent
        anchors.topMargin: -area.topInset
        interactive: false
        clip: true

        TextArea.flickable: TextArea {
            id: area

            /*! The text as last shown or written. */
            property string shownText: ""

            /*! Whether the text is being assigned, not edited. */
            property bool showingText: false

            /*! The text of the document as TextArea reports it, after its own changes. */
            property string documentText: ""

            /*! Whether an edit brought line breaks that are still to be removed. */
            property bool lineBreaksPending: false

            objectName: "editArea"

            focus: true
            wrapMode: TextEdit.NoWrap
            textFormat: TextEdit.PlainText
            selectByMouse: true
            Accessible.name: root.Accessible.name
            onCursorRectangleChanged: root.followCursor()
            onTextChanged: {
                if (area.showingText || area.text === area.documentText)
                    return
                area.documentText = area.text
                if (/[\r\n]/.test(area.text)) {
                    area.lineBreaksPending = true
                    Qt.callLater(root.removeLineBreaks)
                    return
                }
                area.shownText = area.text
                root.textEdited(area.text)
            }
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    event.accepted = true
                } else if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab) {
                    event.accepted = true
                    const forward = event.key === Qt.Key_Tab
                    area.nextItemInFocusChain(forward).forceActiveFocus(Qt.TabFocusReason)
                }
            }
        }
    }
}
