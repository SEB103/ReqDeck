// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

/*!
    \qmltype FormDataTable
    \inqmlmodule ReqDeck
    \brief Edits the parts of a multipart/form-data body: text fields and files.

    The model is a \c FormPartModel; a row's role numbers follow
    FormPartModel::Role.
*/
ColumnLayout {
    id: table

    /*! The FormPartModel shown. */
    property var model: null

    /*! Role numbers of FormPartModel. */
    readonly property int keyRole: 257
    /*! Role number of the value. */
    readonly property int valueRole: 258
    /*! Role number of the enabled flag. */
    readonly property int enabledRole: 259
    /*! Role number of the file flag. */
    readonly property int isFileRole: 260
    /*! Role number of the Content-Type. */
    readonly property int contentTypeRole: 261

    spacing: 4

    Label {
        objectName: "formDataEmpty"

        Layout.fillWidth: true
        visible: partList.count === 0
        wrapMode: Text.Wrap
        opacity: 0.7
        text: qsTr("Add a text field, or a file to upload.")
    }

    ListView {
        id: partList

        objectName: "formDataList"

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 2
        model: table.model
        Accessible.name: qsTr("Form data parts")

        ScrollBar.vertical: ScrollBar {}

        delegate: RowLayout {
            id: part

            required property int index
            required property string key
            required property string value
            required property bool enabled
            required property bool isFile
            required property string contentType

            width: ListView.view.width
            spacing: 4

            CheckBox {
                checked: part.enabled
                Accessible.name: qsTr("Send %1").arg(part.key)
                onToggled: table.model.setField(part.index, table.enabledRole, checked)
            }

            TextField {
                objectName: "partKey"

                Layout.preferredWidth: 140
                placeholderText: qsTr("Field")
                selectByMouse: true
                text: part.key
                Accessible.name: qsTr("Field name")
                onTextEdited: table.model.setField(part.index, table.keyRole, text)
            }

            ComboBox {
                objectName: "partKind"

                Layout.preferredWidth: 90
                model: [qsTr("Text"), qsTr("File")]
                currentIndex: part.isFile ? 1 : 0
                Accessible.name: qsTr("Text or file")
                onActivated: (index) => table.model.setField(part.index, table.isFileRole, index === 1)
            }

            TextField {
                objectName: "partValue"

                Layout.fillWidth: true
                placeholderText: part.isFile ? qsTr("Path of the file") : qsTr("Value")
                selectByMouse: true
                text: part.value
                Accessible.name: part.isFile ? qsTr("File of %1").arg(part.key) : qsTr("Value of %1").arg(part.key)
                onTextEdited: table.model.setField(part.index, table.valueRole, text)
            }

            Button {
                visible: part.isFile
                text: qsTr("Browse…")
                onClicked: {
                    partFileDialog.row = part.index
                    partFileDialog.open()
                }
            }

            TextField {
                Layout.preferredWidth: 130
                placeholderText: qsTr("Type (automatic)")
                selectByMouse: true
                text: part.contentType
                Accessible.name: qsTr("Content-Type of %1").arg(part.key)
                onTextEdited: table.model.setField(part.index, table.contentTypeRole, text)
            }

            ToolButton {
                objectName: "removePart"

                icon.source: "qrc:/images/svg/close.svg"
                Accessible.name: qsTr("Remove %1").arg(part.key)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Remove")
                onClicked: table.model.removeRow(part.index)
            }
        }
    }

    RowLayout {
        spacing: 6

        Button {
            objectName: "addTextPart"

            text: qsTr("Add Field")
            onClicked: table.model.addRow()
        }

        Button {
            objectName: "addFilePart"

            text: qsTr("Add File…")
            onClicked: {
                partFileDialog.row = -1
                partFileDialog.open()
            }
        }
    }

    FileDialog {
        id: partFileDialog

        /*! Row whose file is chosen, or -1 for a new row. */
        property int row: -1

        title: qsTr("Choose a file to upload")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            if (row < 0)
                table.model.addFile(selectedFile.toString())
            else
                table.model.setField(row, table.valueRole, cppWorkspaceManager.toLocalPath(selectedFile.toString()))
        }
    }
}
