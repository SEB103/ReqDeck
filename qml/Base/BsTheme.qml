// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQuick

/*!
    \qmltype BsTheme
    \inqmlmodule Base
    \brief Shared accent tokens and chrome colors for the light and dark themes.

    The application accent is magenta. It is defined once here as four tokens
    (\l accent, \l accentStrong, \l accentSoft, \l accentText), each with a light
    and a dark value; the window binds \c Material.accent to \l accent, and every
    other chrome color below is derived from the tokens. No component outside this
    singleton spells an accent color literal.

    The workspace chrome (pane header strips, the menu band, dividers, the
    selected-row tint, and the status bar) takes its colors from this singleton
    instead of deriving them from \c Material.background, because lightening the
    near-white light background has no visible effect. Content colors are not part
    of this palette: body highlighting, method colors, and HTTP status colors keep
    their own definitions.

    The window binds \l dark to its Material theme. Components that are created
    without a window (for example in tests) see the light palette.
*/
QtObject {
    id: theme

    /*! Whether the dark palette is active. */
    property bool dark: false

    /*! Application accent: Material accent, accent lines, and checked controls. */
    readonly property color accent: theme.dark ? "#F06292" : "#C2185B"

    /*! Stronger accent shade for pressed states and emphasis next to \l accent. */
    readonly property color accentStrong: theme.dark ? "#F48FB1" : "#AD1457"

    /*! Soft accent shade used for light tints and highlight fills. */
    readonly property color accentSoft: theme.dark ? "#880E4F" : "#F48FB1"

    /*!
        Accent color for pane titles and other small chrome text.

        The light value is darker than \l accent so 12 px text keeps a contrast of
        at least 4.5:1 on the light chrome.
    */
    readonly property color accentText: theme.dark ? "#F48FB1" : "#AD1457"

    /*! Background of the pane header strips. */
    readonly property color headerColor: theme.dark ? "#2A2327" : "#F7F0F3"

    /*! Text color of pane titles and table column titles. */
    readonly property color accentTextColor: theme.accentText

    /*! Background of table column header rows. */
    readonly property color tableHeaderColor: theme.dark ? "#262024" : "#F8F2F5"

    /*! Background of the selected tree or list row. */
    readonly property color selectedRowColor: Qt.alpha(theme.accent, theme.dark ? 0.18 : 0.14)

    /*! Color of pane borders and separator lines. */
    readonly property color dividerColor: theme.dark ? "#3B3337" : "#E6DCE1"

    /*! Background of the top band that holds the menu bar and the quick actions. */
    readonly property color menuBandColor: theme.dark ? "#242023" : "#FAF5F7"

    /*! Background of a highlighted or open menu bar title. */
    readonly property color menuHighlightColor: Qt.alpha(theme.accentSoft, theme.dark ? 0.45 : 0.35)

    /*! Background of a highlighted item in a dropdown or context menu. */
    readonly property color menuItemHighlightColor: Qt.alpha(theme.accent, theme.dark ? 0.16 : 0.10)

    /*! Thin accent line under the open menu bar title and on an active status bar. */
    readonly property color accentLineColor: theme.accent

    /*! Color of a split handle while the pointer hovers over it. */
    readonly property color splitHandleHoverColor: Qt.alpha(theme.accent, theme.dark ? 0.6 : 0.55)

    /*!
        Returns the status bar background.

        \a active is true while an operation (for example a request) is running;
        the bar is then tinted with the accent, otherwise it uses the neutral menu
        band color.
    */
    function statusBarColor(active) {
        if (active)
            return Qt.tint(theme.menuBandColor, Qt.alpha(theme.accent, theme.dark ? 0.12 : 0.08))
        return theme.menuBandColor
    }

    /*!
        Returns the color of the line along the top edge of the status bar.

        The line takes the accent while \a active is true and is a plain divider
        otherwise. Use \l statusLineWidth() for its thickness.
    */
    function statusLineColor(active) {
        return active ? theme.accentLineColor : theme.dividerColor
    }

    /*!
        Returns the thickness in pixels of the status bar top line.

        The line is 2 px while \a active is true and the plain 1 px divider
        otherwise.
    */
    function statusLineWidth(active) {
        return active ? 2 : 1
    }
}
