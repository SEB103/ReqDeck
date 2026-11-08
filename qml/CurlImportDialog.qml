// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype CurlImportDialog
    \inqmlmodule ReqDeck
    \brief Makes a new request of a cURL command.

    The command is checked as it changes: Import is enabled only for a command
    that can be converted, and the dialog names what is wrong or which options
    have no equivalent. A pasted command is outside text and shows as plain text.
*/
Dialog {
    id: root

    /*! Topic of the F1 help. */
    property string helpTopic: "curl.input"

    /*! Folder of the new request, or an empty string for the top level. */
    property string parentId: ""

    /*! Outcome of the last check, as returned by RequestEditor::checkCurl(). */
    property var check: ({})

    /*!
        Emitted after the command became a new request.
        \a id is the id of that request in the collection.
    */
    signal imported(string id)

    /*! Checks the command in the text area. */
    function runCheck() {
        root.check = cppRequestEditor.checkCurl(curlInput.text)
    }

    /*! Returns the line describing the last check. */
    function statusText() {
        if (curlInput.length === 0)
            return qsTr("Paste a cURL command copied from a shell or a browser.")
        if (!root.check.ok)
            return root.check.error
        if (root.check.ignored.length === 0)
            return root.check.summary
        // Joined, not chained through arg(): a URL may hold %2 itself.
        return root.check.summary + "\n" + qsTr("Ignored: %1").arg(root.check.ignored.join(" "))
    }

    objectName: "curlImportDialog"

    anchors.centerIn: Overlay.overlay
    modal: true
    Material.roundedScale: Material.SmallScale
    title: qsTr("Import cURL")
    closePolicy: Popup.CloseOnEscape

    onAboutToShow: {
        curlInput.text = ""
        root.runCheck()
    }
    onOpened: curlInput.forceActiveFocus()
    onAccepted: {
        const id = cppRequestEditor.importCurl(curlInput.text, root.parentId)
        if (id.length > 0)
            root.imported(id)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TextArea {
                id: curlInput

                objectName: "curlInput"

                wrapMode: TextEdit.WrapAnywhere
                textFormat: TextEdit.PlainText
                selectByMouse: true
                font.family: "Consolas"
                placeholderText: qsTr("curl https://example.com")
                Accessible.name: qsTr("cURL command")
                // Pasting reports once, and the check runs when the paste is done.
                onTextChanged: Qt.callLater(root.runCheck)
            }
        }

        Label {
            objectName: "curlStatus"

            Layout.fillWidth: true
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 4
            elide: Text.ElideRight
            text: root.statusText()
        }
    }

    footer: DialogButtonBox {
        Button {
            /*! Topic of the F1 help. */
            property string helpTopic: "curl.import"

            objectName: "importButton"

            flat: true
            enabled: root.check.ok === true
            text: qsTr("Import")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }

        Button {
            flat: true
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
