// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsTabButton
    \inqmlmodule Base
    \brief Tab button at the compact bar height.

    The tab bar holding it sets its \c contentHeight to the same
    \l {BsTheme}{BsTheme.compactBarHeight}.
*/
TabButton {
    implicitHeight: BsTheme.compactBarHeight
}
