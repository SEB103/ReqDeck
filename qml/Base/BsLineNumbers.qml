// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsLineNumbers
    \inqmlmodule Base
    \brief Line number gutter for a text that does not wrap.

    Stands beside the text, not inside it, and only builds the rows in view, so
    its cost does not grow with the length of the text. It assumes lines of equal
    height, which holds for one font without wrapping. Taller glyphs from a
    fallback font, such as emoji, make the numbers drift from their lines.
*/
Item {
    id: root

    /*! The view that scrolls the text. */
    property Flickable view: null

    /*!
        Height of one line, as the text item lays it out.
        The host sets it once a layout is done, not with a binding: the height of
        a large text changes all the time while it is colored, and each change
        would rebuild the rows.
    */
    property real lineHeight: 0

    /*! Number of lines of the text. */
    property int lineCount: 0

    /*! Distance from the top of the text item to its first line. */
    property real textTopPadding: 0

    /*! Font of the numbers, the same as of the text. */
    property alias font: metrics.font

    /*! Row of the first line in view. */
    readonly property int firstLine: root.view && root.lineHeight > 0
        ? Math.max(0, Math.floor((root.view.contentY - root.textTopPadding) / root.lineHeight)) : 0

    /*! Whether numbers are shown; the gutter keeps its width either way. */
    property bool showNumbers: true

    /*! Number of rows built. */
    readonly property int visibleLines: root.showNumbers && root.view && root.lineHeight > 0
        ? Math.max(0, Math.min(root.lineCount - root.firstLine,
                               Math.ceil(root.height / root.lineHeight) + 2)) : 0

    /*! Returns the position of the top of \a line in the gutter. */
    function lineY(line) {
        return root.textTopPadding + line * root.lineHeight - (root.view ? root.view.contentY : 0)
    }

    // The width never changes: a change would make the text beside it lay out the
    // whole document again, which took seconds on a long body.
    implicitWidth: metrics.advanceWidth + 12
    clip: true

    TextMetrics {
        id: metrics

        text: "000000"
    }

    Repeater {
        model: root.visibleLines

        delegate: Text {
            required property int index

            y: root.lineY(root.firstLine + index)
            width: root.width - 8
            horizontalAlignment: Text.AlignRight
            font: root.font
            color: Material.foreground
            opacity: 0.55
            text: root.firstLine + index + 1
        }
    }
}
