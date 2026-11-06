// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsKeyValueTable
    \inqmlmodule Base
    \brief Editable table of key/value rows with an enable switch per row.

    Used for query parameters, headers, and form fields. The \l model needs the
    roles \c key, \c value, and \c enabled, and the invokable functions
    \c addRow() and \c removeRow(row). Edits are written to the model as the user
    types, never when the model itself changes. With \l showSecret, the model also
    needs the role \c secret, and the values of secret rows are masked.
*/
ColumnLayout {
    id: root

    /*! Model holding the rows. */
    property var model: null

    /*! Whether each row offers a Secret box. */
    property bool showSecret: false

    /*! Placeholder of the key fields. */
    property string keyPlaceholder: qsTr("Key")

    /*! Placeholder of the value fields. */
    property string valuePlaceholder: qsTr("Value")

    spacing: 4

    ListView {
        objectName: "rows"

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 2
        model: root.model

        ScrollBar.vertical: ScrollBar {}

        delegate: RowLayout {
            id: row

            required property var model
            required property int index

            width: ListView.view.width
            spacing: 4

            CheckBox {
                objectName: "enabledBox"

                checked: row.model.enabled
                Accessible.name: qsTr("Enabled")
                onToggled: row.model.enabled = checked
            }

            TextField {
                objectName: "keyField"

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                placeholderText: root.keyPlaceholder
                selectByMouse: true
                text: row.model.key
                onTextEdited: row.model.key = text
            }

            TextField {
                objectName: "valueField"

                Layout.fillWidth: true
                Layout.preferredWidth: 2
                placeholderText: root.valuePlaceholder
                selectByMouse: true
                echoMode: root.showSecret && row.model.secret ? TextInput.Password : TextInput.Normal
                text: row.model.value
                onTextEdited: row.model.value = text
            }

            CheckBox {
                objectName: "secretBox"

                visible: root.showSecret
                checked: root.showSecret && row.model.secret
                text: qsTr("Secret")
                onToggled: row.model.secret = checked
            }

            ToolButton {
                objectName: "removeButton"

                display: AbstractButton.IconOnly
                icon.source: "qrc:/images/svg/close.svg"
                icon.width: 18
                icon.height: 18
                Accessible.name: qsTr("Remove row")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Remove row")
                onClicked: root.model.removeRow(row.index)
            }
        }
    }

    Button {
        objectName: "addButton"

        flat: true
        text: qsTr("Add row")
        onClicked: root.model.addRow()
    }
}
