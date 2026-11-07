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

    Used for query parameters, headers, and form fields. Columns are named by
    titles above the rows, and \l emptyHint explains an empty table. The \l model needs the
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

    /*! Title of the key column. */
    property string keyTitle: qsTr("Key")

    /*! Title of the value column. */
    property string valueTitle: qsTr("Value")

    /*! Text shown while the table has no rows. */
    property string emptyHint: ""

    /*! Width of the column with the enable switch and of the one with the remove button. */
    readonly property int sideColumnWidth: 40

    /*! Width of the column with the Secret box. */
    readonly property int secretColumnWidth: 90

    spacing: 4

    RowLayout {
        objectName: "columnTitles"

        Layout.fillWidth: true
        spacing: 4

        Item {
            Layout.preferredWidth: root.sideColumnWidth
        }

        Label {
            objectName: "keyColumnTitle"

            Layout.fillWidth: true
            Layout.preferredWidth: 1
            leftPadding: 8
            font.pixelSize: 12
            opacity: 0.7
            text: root.keyTitle
        }

        Label {
            objectName: "valueColumnTitle"

            Layout.fillWidth: true
            Layout.preferredWidth: 2
            leftPadding: 8
            font.pixelSize: 12
            opacity: 0.7
            text: root.valueTitle
        }

        Item {
            Layout.preferredWidth: root.secretColumnWidth
            visible: root.showSecret
        }

        Item {
            Layout.preferredWidth: root.sideColumnWidth
        }
    }

    ListView {
        id: rowList

        objectName: "rows"

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 2
        model: root.model

        ScrollBar.vertical: ScrollBar {}

        Label {
            objectName: "emptyHint"

            parent: rowList
            anchors.centerIn: parent
            width: parent.width - 32
            visible: rowList.count === 0 && root.emptyHint.length > 0
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            opacity: 0.6
            text: root.emptyHint
        }

        delegate: RowLayout {
            id: row

            required property var model
            required property int index

            width: ListView.view.width
            spacing: 4

            CheckBox {
                objectName: "enabledBox"

                Layout.preferredWidth: root.sideColumnWidth
                checked: row.model.enabled
                Accessible.name: qsTr("Enabled")
                onToggled: row.model.enabled = checked
            }

            TextField {
                objectName: "keyField"

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: 34
                topPadding: 4
                bottomPadding: 4
                selectByMouse: true
                Accessible.name: root.keyTitle + " " + (row.index + 1)
                text: row.model.key
                onTextEdited: row.model.key = text
            }

            TextField {
                objectName: "valueField"

                Layout.fillWidth: true
                Layout.preferredWidth: 2
                implicitHeight: 34
                topPadding: 4
                bottomPadding: 4
                selectByMouse: true
                Accessible.name: root.valueTitle + " " + (row.index + 1)
                echoMode: root.showSecret && row.model.secret ? TextInput.Password : TextInput.Normal
                text: row.model.value
                onTextEdited: row.model.value = text
            }

            CheckBox {
                objectName: "secretBox"

                Layout.preferredWidth: root.secretColumnWidth
                visible: root.showSecret
                checked: root.showSecret && row.model.secret
                text: qsTr("Secret")
                onToggled: row.model.secret = checked
            }

            ToolButton {
                objectName: "removeButton"

                Layout.preferredWidth: root.sideColumnWidth
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
