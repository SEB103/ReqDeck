// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

/*!
    \qmltype ImportDialog
    \inqmlmodule ReqDeck
    \brief Imports a collection or environment of another tool, after saying what carries over.

    Uses \c cppImport. Nothing changes in the workspace until Import is pressed.
*/
Dialog {
    id: dialog

    /*! Topic of the F1 help. */
    property string helpTopic: "dialog.import"

    /*! Emitted after an import that added the folder \a id; empty when only environments came. */
    signal imported(string id)

    /*! Reads the file \a fileUrl and shows what it holds. */
    function previewFile(fileUrl) {
        cppImport.preview(fileUrl.toString())
        dialog.open()
    }

    objectName: "importDialog"
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 80 : 640, 640)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 80 : 480, 480)
    modal: true
    Material.roundedScale: Material.SmallScale
    title: qsTr("Import")
    standardButtons: Dialog.Cancel

    footer: DialogButtonBox {
        Button {
            objectName: "importButton"

            text: qsTr("Import")
            enabled: cppImport.ready
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }

        Button {
            text: qsTr("Choose File…")
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: fileDialog.open()
        }
    }

    onAccepted: dialog.imported(cppImport.apply())

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            objectName: "importSummary"

            Layout.fillWidth: true
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            font.bold: cppImport.ready
            text: cppImport.ready ? cppImport.summary
                                  : cppImport.errorString.length > 0 ? cppImport.errorString
                                                                     : qsTr("Choose a Postman collection or environment file.")
        }

        Label {
            Layout.fillWidth: true
            visible: cppImport.ready
            wrapMode: Text.Wrap
            text: cppImport.report.length > 0
                  ? qsTr("These parts will not carry over; everything else will:")
                  : qsTr("Everything carries over. The collection becomes a new folder; nothing is replaced.")
        }

        ListView {
            objectName: "importReport"

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: cppImport.ready && cppImport.report.length > 0
            clip: true
            model: cppImport.report

            ScrollBar.vertical: ScrollBar {}

            delegate: Label {
                required property string modelData

                width: ListView.view.width
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: "• " + modelData
            }
        }
    }

    FileDialog {
        id: fileDialog

        title: qsTr("Import a file")
        nameFilters: [qsTr("Postman files (*.json)"), qsTr("All files (*)")]
        onAccepted: cppImport.preview(selectedFile.toString())
    }
}
