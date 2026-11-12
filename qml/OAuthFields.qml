// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*!
    \qmltype OAuthFields
    \inqmlmodule ReqDeck
    \brief Edits OAuth 2.0 settings: the grant, the URLs, the client, scope, and audience.

    Shows \l settings, a map as RequestEditor::oauth2 and FolderEditor::oauth2
    give it, and reports each edit through fieldEdited().
*/
ColumnLayout {
    id: fields

    /*! Topic of the F1 help. */
    property string helpTopic: "request.oauth"
    /*! The settings shown: grant, authUrl, tokenUrl, clientId, clientSecret, scope, audience. */
    property var settings: ({})

    /*! Emitted when the user changes the field \a name to \a value. */
    signal fieldEdited(string name, var value)

    spacing: 6

    ComboBox {
        objectName: "oauthGrant"

        Layout.preferredWidth: 240
        // The order follows OAuthGrant, so the index is the value.
        model: [qsTr("Authorization code (browser)"), qsTr("Client credentials")]
        currentIndex: fields.settings.grant !== undefined ? fields.settings.grant : 0
        Accessible.name: qsTr("How the token is obtained")
        onActivated: (index) => fields.fieldEdited("grant", index)
    }

    TextField {
        objectName: "oauthAuthUrl"

        Layout.fillWidth: true
        visible: fields.settings.grant !== 1 // OAuthGrant::ClientCredentials
        placeholderText: qsTr("Authorization URL")
        selectByMouse: true
        text: fields.settings.authUrl || ""
        Accessible.name: placeholderText
        onTextEdited: fields.fieldEdited("authUrl", text)
    }

    TextField {
        objectName: "oauthTokenUrl"

        Layout.fillWidth: true
        placeholderText: qsTr("Token URL")
        selectByMouse: true
        text: fields.settings.tokenUrl || ""
        Accessible.name: placeholderText
        onTextEdited: fields.fieldEdited("tokenUrl", text)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        TextField {
            objectName: "oauthClientId"

            Layout.fillWidth: true
            placeholderText: qsTr("Client ID")
            selectByMouse: true
            text: fields.settings.clientId || ""
            Accessible.name: placeholderText
            onTextEdited: fields.fieldEdited("clientId", text)
        }

        TextField {
            objectName: "oauthClientSecret"

            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("Client secret, for example {{clientSecret}}")
            selectByMouse: true
            text: fields.settings.clientSecret || ""
            Accessible.name: qsTr("Client secret")
            onTextEdited: fields.fieldEdited("clientSecret", text)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        TextField {
            objectName: "oauthScope"

            Layout.fillWidth: true
            placeholderText: qsTr("Scope, for example read write")
            selectByMouse: true
            text: fields.settings.scope || ""
            Accessible.name: qsTr("Scope")
            onTextEdited: fields.fieldEdited("scope", text)
        }

        TextField {
            objectName: "oauthAudience"

            Layout.fillWidth: true
            placeholderText: qsTr("Audience (optional)")
            selectByMouse: true
            text: fields.settings.audience || ""
            Accessible.name: qsTr("Audience")
            onTextEdited: fields.fieldEdited("audience", text)
        }
    }
}
