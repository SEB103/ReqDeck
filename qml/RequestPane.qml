// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype RequestPane
    \inqmlmodule ReqDeck
    \brief Edits the request open in \c cppRequestEditor.

    Controls write only on user input (\c onTextEdited, \c onActivated), so showing
    a request never changes it. The name field and the timeout box write when
    editing finishes, because the editor trims names and refuses blank ones; the
    collection pane therefore moves the focus before it loads another request.
    The numbers used for body mode, raw body type, and authentication are the
    values of BodyMode, RawBodyType, and AuthType.
*/
Item {
    id: root

    /*!
        Sends the open request from a shortcut.
        Taking the focus stores a name or timeout still being edited, and giving it
        back keeps the user where they were typing.
    */
    function sendNow() {
        const focused = root.Window.activeFocusItem
        root.forceActiveFocus(Qt.ShortcutFocusReason)
        cppRequestEditor.send()
        if (focused)
            focused.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    Binding {
        target: cppEnvironments
        property: "darkTheme"
        value: Base.BsTheme.dark
    }

    Shortcut {
        sequences: ["Ctrl+Return", "Ctrl+Enter"]
        enabled: cppRequestEditor.canSend
        onActivated: root.sendNow()
    }

    Label {
        objectName: "placeholder"

        anchors.centerIn: parent
        width: parent.width - 32
        visible: !cppRequestEditor.hasRequest
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        opacity: 0.6
        text: qsTr("Select or create a request to edit it here.")
    }

    ColumnLayout {
        objectName: "editor"

        anchors.fill: parent
        anchors.margins: 8
        visible: cppRequestEditor.hasRequest
        spacing: 6

        TextField {
            objectName: "nameField"

            Layout.fillWidth: true
            placeholderText: qsTr("Request name")
            selectByMouse: true
            text: cppRequestEditor.name
            onEditingFinished: cppRequestEditor.name = text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            ComboBox {
                objectName: "methodCombo"

                Layout.preferredWidth: 120
                model: ["GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"]
                currentIndex: Math.max(0, indexOfValue(cppRequestEditor.method))
                onActivated: cppRequestEditor.method = currentText
            }

            Base.BsSingleLineEdit {
                objectName: "urlField"

                Layout.fillWidth: true
                placeholderText: qsTr("https://example.com or {{baseUrl}}/path")
                text: cppRequestEditor.url
                Accessible.name: qsTr("URL")
                onTextEdited: (edited) => cppRequestEditor.url = edited
                Component.onCompleted: cppEnvironments.attachUrlHighlighter(textDocument)
            }

            // Taking the focus first commits a field still being edited.
            Button {
                objectName: "sendButton"

                highlighted: true
                text: cppRequestEditor.busy ? qsTr("Cancel") : qsTr("Send")
                onClicked: {
                    forceActiveFocus(Qt.MouseFocusReason)
                    if (cppRequestEditor.busy)
                        cppRequestEditor.cancel()
                    else
                        cppRequestEditor.send()
                }
            }
        }

        TabBar {
            id: tabs

            objectName: "tabs"

            Layout.fillWidth: true
            contentHeight: Base.BsTheme.compactBarHeight

            Base.BsTabButton {
                text: qsTr("Params")
            }

            Base.BsTabButton {
                text: qsTr("Headers")
            }

            Base.BsTabButton {
                text: qsTr("Body")
            }

            Base.BsTabButton {
                text: qsTr("Auth")
            }
        }

        StackLayout {
            objectName: "tabStack"

            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            Base.BsKeyValueTable {
                model: cppRequestEditor.paramsModel
                keyPlaceholder: qsTr("Parameter")
            }

            Base.BsKeyValueTable {
                objectName: "headersTable"

                model: cppRequestEditor.headersModel
                keyPlaceholder: qsTr("Header")
            }

            ColumnLayout {
                spacing: 6

                RowLayout {
                    spacing: 6

                    ComboBox {
                        Layout.preferredWidth: 180
                        model: [qsTr("No body"), qsTr("Raw"), qsTr("Form URL encoded")]
                        currentIndex: cppRequestEditor.bodyMode
                        onActivated: (index) => cppRequestEditor.bodyMode = index
                    }

                    ComboBox {
                        Layout.preferredWidth: 120
                        visible: cppRequestEditor.bodyMode === 1 // BodyMode::Raw
                        model: ["JSON", qsTr("Text"), "XML"]
                        currentIndex: cppRequestEditor.rawBodyType
                        onActivated: (index) => cppRequestEditor.rawBodyType = index
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: cppRequestEditor.bodyMode === 1 // BodyMode::Raw

                    // TextArea has no textEdited signal, and it normalizes line
                    // endings, so a binding could not tell an edit from showing
                    // a request. The text is assigned here instead.
                    TextArea {
                        id: rawBodyArea

                        /*! The request's body as last shown or written. */
                        property string shownBody: ""

                        /*! Whether the text is being replaced by the request's body. */
                        property bool showingBody: false

                        /*! Shows the body of the request unless it is already shown. */
                        function showBody() {
                            if (cppRequestEditor.rawBody === rawBodyArea.shownBody)
                                return
                            rawBodyArea.shownBody = cppRequestEditor.rawBody
                            rawBodyArea.showingBody = true
                            rawBodyArea.text = cppRequestEditor.rawBody
                            rawBodyArea.showingBody = false
                        }

                        objectName: "rawBodyArea"

                        font.family: "Consolas"
                        selectByMouse: true
                        placeholderText: qsTr("Request body")
                        onTextChanged: {
                            if (rawBodyArea.showingBody)
                                return
                            rawBodyArea.shownBody = text
                            cppRequestEditor.rawBody = text
                        }
                        Component.onCompleted: rawBodyArea.showBody()

                        Connections {
                            target: cppRequestEditor

                            function onRequestChanged() {
                                rawBodyArea.showBody()
                            }
                        }
                    }
                }

                Base.BsKeyValueTable {
                    objectName: "formTable"

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: cppRequestEditor.bodyMode === 2 // BodyMode::FormUrlEncoded
                    model: cppRequestEditor.formModel
                    keyPlaceholder: qsTr("Field")
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: cppRequestEditor.bodyMode === 0 // BodyMode::None
                }
            }

            ColumnLayout {
                spacing: 6

                ComboBox {
                    Layout.preferredWidth: 180
                    model: [qsTr("No authentication"), qsTr("Basic"), qsTr("Bearer token"), qsTr("API key")]
                    currentIndex: cppRequestEditor.authType
                    onActivated: (index) => cppRequestEditor.authType = index
                }

                TextField {
                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 1 // AuthType::Basic
                    placeholderText: qsTr("User name")
                    selectByMouse: true
                    text: cppRequestEditor.authUsername
                    onTextEdited: cppRequestEditor.authUsername = text
                }

                TextField {
                    objectName: "passwordField"

                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 1 // AuthType::Basic
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Password")
                    selectByMouse: true
                    text: cppRequestEditor.authPassword
                    onTextEdited: cppRequestEditor.authPassword = text
                }

                TextField {
                    objectName: "tokenField"

                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 2 // AuthType::Bearer
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Token")
                    selectByMouse: true
                    text: cppRequestEditor.authToken
                    onTextEdited: cppRequestEditor.authToken = text
                }

                TextField {
                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 3 // AuthType::ApiKey
                    placeholderText: qsTr("Key name")
                    selectByMouse: true
                    text: cppRequestEditor.apiKeyName
                    onTextEdited: cppRequestEditor.apiKeyName = text
                }

                TextField {
                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 3 // AuthType::ApiKey
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Key value")
                    selectByMouse: true
                    text: cppRequestEditor.apiKeyValue
                    onTextEdited: cppRequestEditor.apiKeyValue = text
                }

                ComboBox {
                    Layout.preferredWidth: 180
                    visible: cppRequestEditor.authType === 3 // AuthType::ApiKey
                    model: [qsTr("Send as header"), qsTr("Send as query parameter")]
                    currentIndex: cppRequestEditor.apiKeyInQuery ? 1 : 0
                    onActivated: (index) => cppRequestEditor.apiKeyInQuery = index === 1
                }

                RowLayout {
                    spacing: 6

                    Label {
                        text: qsTr("Timeout in ms (0 = default)")
                    }

                    SpinBox {
                        objectName: "timeoutBox"

                        editable: true
                        from: 0
                        to: 3600000
                        stepSize: 1000
                        value: cppRequestEditor.timeoutMs
                        onValueModified: cppRequestEditor.timeoutMs = value
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }
}
