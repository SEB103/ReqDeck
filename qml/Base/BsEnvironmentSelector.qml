// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

/*!
    \qmltype BsEnvironmentSelector
    \inqmlmodule Base
    \brief Combo box choosing the active environment, or none.

    The host supplies the environment names and the active row. The first row of
    the box stands for no environment, so its rows are one higher than the rows
    of \l names. The names come from a workspace file and show as plain text.
*/
ComboBox {
    id: root

    /*! Names of the environments. */
    property var names: []

    /*! Row in \l names of the active environment, or -1 when none is active. */
    property int activeIndex: -1

    /*!
        Emitted when the user chooses a row.
        \a index is the row in \l names, or -1 for no environment.
    */
    signal environmentSelected(int index)

    /*!
        Selects the row of the active environment.
        A new model resets the current index, so this also runs after the names
        change, and assigning here keeps it from depending on a binding.
    */
    function showActive() {
        root.currentIndex = root.activeIndex + 1
    }

    model: [qsTr("No environment")].concat(root.names)
    Accessible.name: qsTr("Environment")
    onModelChanged: root.showActive()
    onActiveIndexChanged: root.showActive()
    onActivated: (index) => root.environmentSelected(index - 1)

    delegate: ItemDelegate {
        id: option

        required property string modelData
        required property int index

        width: ListView.view.width
        highlighted: root.highlightedIndex === option.index
        Accessible.name: option.modelData

        contentItem: Label {
            verticalAlignment: Text.AlignVCenter
            textFormat: Text.PlainText
            elide: Text.ElideRight
            text: option.modelData
        }
    }
}
