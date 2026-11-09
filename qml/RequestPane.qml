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
        root.trySend()
        if (focused)
            focused.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    /*!
        Sends the open request, or first asks for the values of variables that do
        not resolve, so nothing leaves with a literal {{name}}.
    */
    function trySend() {
        const missing = cppRequestEditor.missingVariables()
        if (missing.length > 0)
            missingValuesDialog.ask(missing)
        else
            cppRequestEditor.send()
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

        Base.BsInlineTitle {
            id: nameTitle

            /*! Topic of the F1 help. */
            property string helpTopic: "request.name"

            objectName: "nameField"

            Layout.fillWidth: true
            text: cppRequestEditor.name
            Accessible.name: qsTr("Request name")
            // The editor trims a name and refuses a blank one, without always
            // reporting a change, so the field shows the stored name again.
            onEditingFinished: {
                cppRequestEditor.name = text
                nameTitle.text = Qt.binding(() => cppRequestEditor.name)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            ComboBox {
                id: methodCombo

                /*! Topic of the F1 help. */
                property string helpTopic: "request.method"

                objectName: "methodCombo"

                Layout.preferredWidth: 108
                Material.foreground: Base.BsTheme.methodColor(cppRequestEditor.method)
                font.bold: true
                model: ["GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"]
                currentIndex: Math.max(0, indexOfValue(cppRequestEditor.method))
                onActivated: cppRequestEditor.method = currentText

                delegate: ItemDelegate {
                    id: methodOption

                    required property string modelData
                    required property int index

                    width: ListView.view.width
                    highlighted: methodCombo.highlightedIndex === methodOption.index
                    text: methodOption.modelData

                    contentItem: Label {
                        verticalAlignment: Text.AlignVCenter
                        font.bold: true
                        color: Base.BsTheme.methodColor(methodOption.text)
                        text: methodOption.text
                    }
                }
            }

            Base.BsSingleLineEdit {
                /*! Topic of the F1 help. */
                property string helpTopic: "request.url"

                id: urlField

                objectName: "urlField"

                Layout.fillWidth: true
                font.family: "Consolas"
                font.pixelSize: 14
                placeholderText: qsTr("https://example.com or {{baseUrl}}/path")
                text: cppRequestEditor.url
                keyFilter: (event) => urlCompleter.handleKey(event)
                Accessible.name: qsTr("URL")
                onTextEdited: (edited) => {
                    cppRequestEditor.url = edited
                    urlCompleter.refresh()
                }
                Component.onCompleted: cppEnvironments.attachUrlHighlighter(textDocument)

                Base.BsVariableCompleter {
                    id: urlCompleter

                    objectName: "urlCompleter"
                    target: urlField
                    names: cppEnvironments.activeVariableNames
                    onCompleted: (text, cursorPosition) => {
                        cppRequestEditor.url = text
                        urlField.cursorPosition = cursorPosition
                    }
                }
            }

            // Taking the focus first commits a field still being edited.
            Button {
                /*! Topic of the F1 help. */
                property string helpTopic: "request.send"

                objectName: "sendButton"

                Layout.preferredHeight: methodCombo.height
                topInset: 0
                bottomInset: 0
                Material.roundedScale: Material.ExtraSmallScale
                Material.elevation: 0
                highlighted: true
                text: cppRequestEditor.busy ? qsTr("Cancel") : qsTr("Send")
                onClicked: {
                    forceActiveFocus(Qt.MouseFocusReason)
                    if (cppRequestEditor.busy)
                        cppRequestEditor.cancel()
                    else
                        root.trySend()
                }
            }
        }

        TabBar {
            id: tabs

            /*! Topic of the F1 help. */
            property string helpTopic: "request.tabs"

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
                /*! Topic of the F1 help. */
                property string helpTopic: "request.params"

                objectName: "paramsTable"

                knownVariables: cppEnvironments.activeVariableNames

                model: cppRequestEditor.paramsModel
                keyTitle: qsTr("Parameter")
                emptyHint: qsTr("Add a parameter to send it in the URL.")
            }

            Base.BsKeyValueTable {
                /*! Topic of the F1 help. */
                property string helpTopic: "request.headers"

                objectName: "headersTable"

                knownVariables: cppEnvironments.activeVariableNames

                model: cppRequestEditor.headersModel
                keyTitle: qsTr("Header")
                emptyHint: qsTr("Add a header to send extra information with the request.")
            }

            ColumnLayout {
                spacing: 6

                RowLayout {
                    spacing: 6

                    ComboBox {
                        /*! Topic of the F1 help. */
                        property string helpTopic: "request.bodyMode"

                        Layout.preferredWidth: 180
                        model: [qsTr("No body"), qsTr("Raw"), qsTr("Form URL encoded")]
                        currentIndex: cppRequestEditor.bodyMode
                        onActivated: (index) => cppRequestEditor.bodyMode = index
                    }

                    ComboBox {
                        /*! Topic of the F1 help. */
                        property string helpTopic: "request.rawType"

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

                        /*! Topic of the F1 help. */
                        property string helpTopic: "request.rawBody"

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
                        Keys.onPressed: (event) => rawBodyCompleter.handleKey(event)
                        onTextChanged: {
                            if (rawBodyArea.showingBody)
                                return
                            rawBodyArea.shownBody = text
                            cppRequestEditor.rawBody = text
                            if (rawBodyArea.activeFocus)
                                rawBodyCompleter.refresh()
                        }
                        Component.onCompleted: rawBodyArea.showBody()

                        Base.BsVariableCompleter {
                            id: rawBodyCompleter

                            target: rawBodyArea
                            names: cppEnvironments.activeVariableNames
                            onCompleted: (text, cursorPosition) => {
                                rawBodyArea.text = text
                                rawBodyArea.cursorPosition = cursorPosition
                            }
                        }

                        Connections {
                            target: cppRequestEditor

                            function onRequestChanged() {
                                rawBodyArea.showBody()
                            }
                        }
                    }
                }

                Base.BsKeyValueTable {
                    /*! Topic of the F1 help. */
                    property string helpTopic: "request.form"

                    objectName: "formTable"

                    knownVariables: cppEnvironments.activeVariableNames

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: cppRequestEditor.bodyMode === 2 // BodyMode::FormUrlEncoded
                    model: cppRequestEditor.formModel
                    keyTitle: qsTr("Field")
                    emptyHint: qsTr("Add a field to send it in the body.")
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
                    /*! Topic of the F1 help. */
                    property string helpTopic: "request.authType"

                    Layout.preferredWidth: 180
                    model: [qsTr("No authentication"), qsTr("Basic"), qsTr("Bearer token"), qsTr("API key")]
                    currentIndex: cppRequestEditor.authType
                    onActivated: (index) => cppRequestEditor.authType = index
                }

                TextField {
                    /*! Topic of the F1 help. */
                    property string helpTopic: "request.authUser"

                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 1 // AuthType::Basic
                    placeholderText: qsTr("User name")
                    selectByMouse: true
                    text: cppRequestEditor.authUsername
                    onTextEdited: cppRequestEditor.authUsername = text
                }

                TextField {
                    /*! Topic of the F1 help. */
                    property string helpTopic: "request.authPassword"

                    id: passwordField

                    /*! Variables in the field that do not resolve. */
                    readonly property var unknown: Base.BsVariables.unknownIn(text, cppEnvironments.activeVariableNames)

                    objectName: "passwordField"

                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 1 // AuthType::Basic
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Password")
                    selectByMouse: true
                    text: cppRequestEditor.authPassword
                    ToolTip.visible: hovered && passwordField.unknown.length > 0
                    ToolTip.text: passwordFieldMark.tooltip
                    Keys.onPressed: (event) => passwordFieldCompleter.handleKey(event)
                    onTextEdited: {
                        cppRequestEditor.authPassword = text
                        passwordFieldCompleter.refresh()
                    }

                    Base.BsVariableCompleter {
                        id: passwordFieldCompleter

                        target: passwordField
                        names: cppEnvironments.activeVariableNames
                        onCompleted: (text, cursorPosition) => {
                            cppRequestEditor.authPassword = text
                            passwordField.cursorPosition = cursorPosition
                        }
                    }

                    Base.BsUnknownVariablesMark {
                        id: passwordFieldMark

                        names: passwordField.unknown
                    }
                }

                TextField {
                    /*! Topic of the F1 help. */
                    property string helpTopic: "request.token"

                    id: tokenField

                    /*! Variables in the field that do not resolve. */
                    readonly property var unknown: Base.BsVariables.unknownIn(text, cppEnvironments.activeVariableNames)

                    objectName: "tokenField"

                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 2 // AuthType::Bearer
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Token")
                    selectByMouse: true
                    text: cppRequestEditor.authToken
                    ToolTip.visible: hovered && tokenField.unknown.length > 0
                    ToolTip.text: tokenFieldMark.tooltip
                    Keys.onPressed: (event) => tokenFieldCompleter.handleKey(event)
                    onTextEdited: {
                        cppRequestEditor.authToken = text
                        tokenFieldCompleter.refresh()
                    }

                    Base.BsVariableCompleter {
                        id: tokenFieldCompleter

                        target: tokenField
                        names: cppEnvironments.activeVariableNames
                        onCompleted: (text, cursorPosition) => {
                            cppRequestEditor.authToken = text
                            tokenField.cursorPosition = cursorPosition
                        }
                    }

                    Base.BsUnknownVariablesMark {
                        id: tokenFieldMark

                        names: tokenField.unknown
                    }
                }

                TextField {
                    /*! Topic of the F1 help. */
                    property string helpTopic: "request.apiKeyName"

                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 3 // AuthType::ApiKey
                    placeholderText: qsTr("Key name")
                    selectByMouse: true
                    text: cppRequestEditor.apiKeyName
                    onTextEdited: cppRequestEditor.apiKeyName = text
                }

                TextField {
                    id: apiKeyValueField

                    /*! Topic of the F1 help. */
                    property string helpTopic: "request.apiKeyValue"
                    /*! Variables in the field that do not resolve. */
                    readonly property var unknown: Base.BsVariables.unknownIn(text, cppEnvironments.activeVariableNames)

                    Layout.fillWidth: true
                    visible: cppRequestEditor.authType === 3 // AuthType::ApiKey
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Key value")
                    selectByMouse: true
                    text: cppRequestEditor.apiKeyValue
                    ToolTip.visible: hovered && unknown.length > 0
                    ToolTip.text: apiKeyValueMark.tooltip
                    Keys.onPressed: (event) => apiKeyValueCompleter.handleKey(event)
                    onTextEdited: {
                        cppRequestEditor.apiKeyValue = text
                        apiKeyValueCompleter.refresh()
                    }

                    Base.BsVariableCompleter {
                        id: apiKeyValueCompleter

                        target: apiKeyValueField
                        names: cppEnvironments.activeVariableNames
                        onCompleted: (text, cursorPosition) => {
                            cppRequestEditor.apiKeyValue = text
                            apiKeyValueField.cursorPosition = cursorPosition
                        }
                    }

                    Base.BsUnknownVariablesMark {
                        id: apiKeyValueMark

                        names: parent.unknown
                    }
                }

                RowLayout {
                    objectName: "credentialHint"

                    Layout.fillWidth: true
                    spacing: 8
                    visible: cppRequestEditor.literalCredential.length > 0

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        color: Material.hintTextColor
                        text: qsTr("This value is saved in the workspace file, which you may share.")
                    }

                    Button {
                        /*! Topic of the F1 help. */
                        property string helpTopic: "request.moveToSecret"

                        objectName: "moveToSecretButton"
                        text: qsTr("Move to a secret")
                        flat: true
                        onClicked: {
                            const key = cppEnvironments.addSecretVariable(cppRequestEditor.literalCredential,
                                                                          cppRequestEditor.literalCredentialValue())
                            cppRequestEditor.useVariableForCredential(key)
                        }
                    }
                }

                ComboBox {
                    /*! Topic of the F1 help. */
                    property string helpTopic: "request.apiKeyPlace"

                    Layout.preferredWidth: 180
                    visible: cppRequestEditor.authType === 3 // AuthType::ApiKey
                    model: [qsTr("Send as header"), qsTr("Send as query parameter")]
                    currentIndex: cppRequestEditor.apiKeyInQuery ? 1 : 0
                    onActivated: (index) => cppRequestEditor.apiKeyInQuery = index === 1
                }

                RowLayout {
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        text: qsTr("Timeout in ms (0 = default)")
                    }

                    SpinBox {
                        /*! Topic of the F1 help. */
                        property string helpTopic: "request.timeout"

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

    Dialog {
        id: missingValuesDialog

        /*! Topic of the F1 help. */
        property string helpTopic: "dialog.missingValues"

        /*! Names of the variables to ask for. */
        property var names: []

        /*! Opens the dialog for \a list of variable names. */
        function ask(list) {
            missingValuesDialog.names = list
            keepBox.checked = false
            missingValuesDialog.open()
        }

        objectName: "missingValuesDialog"
        anchors.centerIn: Overlay.overlay
        width: 460
        modal: true
        Material.roundedScale: Material.SmallScale
        title: qsTr("Values needed")
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            const values = {}
            for (let i = 0; i < valueFields.count; ++i)
                values[missingValuesDialog.names[i]] = valueFields.itemAt(i).text
            if (keepBox.checked) {
                for (const name of missingValuesDialog.names)
                    cppEnvironments.setVariable(name, values[name])
                cppRequestEditor.send()
            } else {
                cppRequestEditor.sendWithValues(values)
            }
        }

        ColumnLayout {
            width: parent.width
            spacing: 8

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("These variables have no value in the active environment. The values are used for this send.")
            }

            Repeater {
                id: valueFields

                model: missingValuesDialog.names

                delegate: TextField {
                    required property string modelData

                    objectName: "missingValueField"
                    Layout.fillWidth: true
                    placeholderText: "{{" + modelData + "}}"
                    selectByMouse: true
                    Accessible.name: modelData
                }
            }

            CheckBox {
                id: keepBox

                objectName: "keepValuesBox"
                text: qsTr("Keep them in the active environment")
            }
        }
    }
}
