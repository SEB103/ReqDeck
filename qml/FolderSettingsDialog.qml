// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype FolderSettingsDialog
    \inqmlmodule ReqDeck
    \brief Edits the auth and variables that the requests of a folder inherit.

    Reads and writes \c cppFolderEditor. Edits apply at once, like the other
    editors; the dialog has Close only.
*/
Dialog {
    id: dialog

    /*! Topic of the F1 help. */
    property string helpTopic: "dialog.folderSettings"

    /*! Values of AuthType in the order of the choices: Inherit first, as a new folder has it. */
    readonly property var authTypes: [4, 0, 1, 2, 3]

    /*! Shows the settings of the folder \a id. */
    function openFor(id) {
        cppFolderEditor.load(id)
        dialog.open()
    }

    objectName: "folderSettingsDialog"
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 80 : 640, 640)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 80 : 560, 560)
    modal: true
    Material.roundedScale: Material.SmallScale
    title: qsTr("Folder settings: %1").arg(cppFolderEditor.name)
    standardButtons: Dialog.Close

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Requests in this folder that inherit their authorization use the one set here. The variables apply to them too; the active environment wins over them.")
        }

        ComboBox {
            objectName: "folderAuthType"

            Layout.preferredWidth: 220
            model: [qsTr("Inherit from parent folder"), qsTr("No authentication"), qsTr("Basic"), qsTr("Bearer token"),
                    qsTr("API key")]
            currentIndex: dialog.authTypes.indexOf(cppFolderEditor.authType)
            onActivated: (index) => cppFolderEditor.authType = dialog.authTypes[index]
        }

        TextField {
            Layout.fillWidth: true
            visible: cppFolderEditor.authType === 1 // AuthType::Basic
            placeholderText: qsTr("User name")
            selectByMouse: true
            text: cppFolderEditor.authUsername
            onTextEdited: cppFolderEditor.authUsername = text
        }

        TextField {
            Layout.fillWidth: true
            visible: cppFolderEditor.authType === 1 // AuthType::Basic
            echoMode: TextInput.Password
            placeholderText: qsTr("Password")
            selectByMouse: true
            text: cppFolderEditor.authPassword
            onTextEdited: cppFolderEditor.authPassword = text
        }

        TextField {
            objectName: "folderToken"

            Layout.fillWidth: true
            visible: cppFolderEditor.authType === 2 // AuthType::Bearer
            echoMode: TextInput.Password
            placeholderText: qsTr("Token, for example {{token}}")
            selectByMouse: true
            text: cppFolderEditor.authToken
            onTextEdited: cppFolderEditor.authToken = text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: cppFolderEditor.authType === 3 // AuthType::ApiKey

            TextField {
                Layout.fillWidth: true
                placeholderText: qsTr("Key name")
                selectByMouse: true
                text: cppFolderEditor.apiKeyName
                onTextEdited: cppFolderEditor.apiKeyName = text
            }

            TextField {
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Key value")
                selectByMouse: true
                text: cppFolderEditor.apiKeyValue
                onTextEdited: cppFolderEditor.apiKeyValue = text
            }

            ComboBox {
                model: [qsTr("Send as header"), qsTr("Send as query parameter")]
                currentIndex: cppFolderEditor.apiKeyInQuery ? 1 : 0
                onActivated: (index) => cppFolderEditor.apiKeyInQuery = index === 1
            }
        }

        Base.BsKeyValueTable {
            objectName: "folderVariables"

            Layout.fillWidth: true
            Layout.fillHeight: true
            model: cppFolderEditor.variablesModel
            keyTitle: qsTr("Variable")
            emptyHint: qsTr("Add a variable for the requests of this folder.")
        }
    }
}
