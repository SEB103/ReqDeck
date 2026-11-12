// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import Base as Base

/*!
    \qmltype ResponsePane
    \inqmlmodule ReqDeck
    \brief Shows the state of a running request and the last response.

    Reads \c cppResponse. The pane names the request a response belongs to,
    because it stays visible when another request is opened.
*/
Item {
    id: root

    /*! Topic of the F1 help. */
    property string helpTopic: "response.body"

    /*! Whether the find bar is shown. */
    property bool findOpen: false

    /*! Start positions of the find text in the body. */
    property var findMatches: []

    /*! Index of the selected match in findMatches, or -1. */
    property int findIndex: -1

    /*! Shows the find bar and puts the cursor in it. */
    function openFind() {
        root.findOpen = true
        findField.forceActiveFocus()
        findField.selectAll()
        // The body may have changed while the bar was hidden.
        root.runFind()
    }

    /*! Hides the find bar and gives the focus back to the body. */
    function closeFind() {
        root.findOpen = false
        bodyArea.deselect()
        bodyArea.forceActiveFocus()
    }

    /*! Looks for the find text again, after it or the body changed, and selects the first match. */
    function runFind() {
        root.findMatches = root.findOpen ? cppResponse.findInBody(findField.text) : []
        root.findIndex = root.findMatches.length > 0 ? 0 : -1
        root.showMatch()
    }

    /*! Selects the match \a step places away, wrapping at both ends. */
    function moveFind(step) {
        const count = root.findMatches.length
        if (count === 0)
            return
        root.findIndex = (root.findIndex + step + count) % count
        root.showMatch()
    }

    /*! Selects the current match and scrolls it into view. */
    function showMatch() {
        if (root.findIndex < 0) {
            bodyArea.deselect()
            return
        }
        const start = root.findMatches[root.findIndex]
        bodyArea.select(start, start + findField.text.length)
        const flickable = bodyScroll.contentItem
        const rect = bodyArea.positionToRectangle(start)
        if (rect.y < flickable.contentY || rect.y + rect.height > flickable.contentY + flickable.height)
            flickable.contentY = Math.max(0, Math.min(rect.y - flickable.height / 3,
                                                      flickable.contentHeight - flickable.height))
        if (rect.x < flickable.contentX || rect.x > flickable.contentX + flickable.width - 40)
            flickable.contentX = Math.max(0, rect.x - flickable.width / 3)
    }

    /*! Returns the count shown in the find bar. */
    function findCountText() {
        const count = root.findMatches.length
        if (findField.text.length === 0)
            return ""
        if (count === 0)
            return qsTr("No matches")
        const total = count >= 10000 ? qsTr("%1+").arg(count) : count
        return qsTr("%1 of %2").arg(root.findIndex + 1).arg(total)
    }

    /*! Saves the body to the local file \a fileUrl; the result is reported by cppResponse. */
    function saveTo(fileUrl) {
        cppResponse.saveBody(fileUrl)
    }

    Binding {
        target: cppResponse
        property: "darkTheme"
        value: Base.BsTheme.dark
    }

    Label {
        objectName: "placeholder"

        anchors.centerIn: parent
        width: parent.width - 32
        visible: !cppResponse.hasResponse && !cppResponse.busy
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        opacity: 0.6
        text: qsTr("Send a request to see the response here.")
    }

    ColumnLayout {
        objectName: "busyColumn"

        anchors.centerIn: parent
        width: parent.width - 32
        visible: cppResponse.busy
        spacing: 8

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: cppResponse.busy
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            textFormat: Text.PlainText
            text: qsTr("Sending %1…").arg(cppResponse.requestName)
        }

        Button {
            /*! Topic of the F1 help. */
            property string helpTopic: "request.send"

            objectName: "cancelButton"

            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Cancel")
            onClicked: cppRequestEditor.cancel()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        visible: cppResponse.hasResponse
        spacing: 6

        // The details wrap, so that they stay inside a narrow pane.
        Flow {
            id: details

            Layout.fillWidth: true
            spacing: 10

            Base.BsStatusChip {
                /*! Topic of the F1 help. */
                property string helpTopic: "response.status"

                objectName: "statusChip"

                width: Math.min(implicitWidth, details.width)
                visible: cppResponse.statusCode > 0
                statusClass: cppResponse.statusClass
                text: (cppResponse.statusCode + " " + cppResponse.reasonPhrase).trim()
            }

            Label {
                objectName: "timeLabel"

                visible: cppResponse.statusCode > 0
                text: qsTr("%1 ms").arg(cppResponse.elapsedMs)
            }

            Label {
                objectName: "sizeLabel"

                visible: cppResponse.statusCode > 0
                text: cppResponse.sizeText
            }

            ToolButton {
                /*! Topic of the F1 help. */
                property string helpTopic: "response.save"

                objectName: "saveResponseButton"

                visible: cppResponse.canSaveBody
                enabled: !cppResponse.busy
                display: AbstractButton.IconOnly
                icon.source: "qrc:/images/svg/save.svg"
                icon.width: 18
                icon.height: 18
                Accessible.name: qsTr("Save response")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Save response to a file")
                onClicked: {
                    // The folder is a URL, so a name with # or % must be encoded to stay one file name.
                    saveDialog.selectedFile = StandardPaths.writableLocation(StandardPaths.DownloadLocation)
                                              + "/" + encodeURIComponent(cppResponse.suggestedFileName())
                    saveDialog.open()
                }
            }
        }

        Label {
            objectName: "requestNameLabel"

            Layout.fillWidth: true
            visible: cppResponse.requestId !== cppRequestEditor.requestId
            elide: Text.ElideRight
            opacity: 0.6
            textFormat: Text.PlainText
            text: cppResponse.requestName
        }

        Label {
            objectName: "errorLabel"

            Layout.fillWidth: true
            visible: cppResponse.errorText.length > 0
            wrapMode: Text.Wrap
            color: Material.color(Material.Red, Base.BsTheme.dark ? Material.Shade300 : Material.Shade800)
            textFormat: Text.PlainText
            text: cppResponse.errorText
        }

        RowLayout {
            /*! Topic of the F1 help. */
            property string helpTopic: "response.tlsBanner"

            objectName: "certificateBanner"

            Layout.fillWidth: true
            // Offered only beside the request that failed, so that Continue
            // always sends what the user saw fail.
            visible: cppResponse.canTrustCertificate && cppResponse.requestId === cppRequestEditor.requestId
            spacing: 8

            Label {
                objectName: "certificateText"

                Layout.fillWidth: true
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: qsTr("The certificate of %1 is not trusted. Continue only for a server you know, such as your own test server.")
                          .arg(cppResponse.certificateHost)
            }

            Button {
                /*! Topic of the F1 help. */
                property string helpTopic: "response.tlsContinue"

                objectName: "trustCertificateButton"

                enabled: !cppRequestEditor.busy
                text: qsTr("Continue")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Send this request once more without checking the certificate")
                onClicked: cppRequestEditor.sendTrustingCertificate()
            }
        }

        Label {
            objectName: "warningLabel"

            Layout.fillWidth: true
            visible: cppResponse.warningText.length > 0
            wrapMode: Text.Wrap
            color: Material.color(Material.Orange, Base.BsTheme.dark ? Material.Shade300 : Material.Shade900)
            textFormat: Text.PlainText
            text: cppResponse.warningText
        }

        TabBar {
            id: responseTabs

            /*! Topic of the F1 help. */
            property string helpTopic: "response.tabs"

            objectName: "responseTabs"

            Layout.fillWidth: true
            contentHeight: Base.BsTheme.compactBarHeight

            Base.BsTabButton {
                text: qsTr("Body")
            }

            Base.BsTabButton {
                text: qsTr("Headers")
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: responseTabs.currentIndex

            ColumnLayout {
                spacing: 4

                TextField {
                    /*! Topic of the F1 help. */
                    property string helpTopic: "response.filter"

                    objectName: "responseFilter"

                    Layout.fillWidth: true
                    visible: cppResponse.canFilter
                    placeholderText: qsTr("Filter with JSONPath, for example $.items[*].id")
                    selectByMouse: true
                    font.family: "Consolas"
                    text: cppResponse.filterText
                    Accessible.name: qsTr("JSONPath filter")
                    onTextEdited: cppResponse.filterText = text
                }

                Label {
                    objectName: "responseFilterError"

                    Layout.fillWidth: true
                    visible: cppResponse.canFilter && cppResponse.filterError.length > 0
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    color: Material.color(Material.Red)
                    text: cppResponse.filterError
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Base.BsSegmentedControl {
                        /*! Topic of the F1 help. */
                        property string helpTopic: "response.pretty"

                        objectName: "prettySwitch"

                        visible: cppResponse.canPretty
                        options: [qsTr("Pretty"), qsTr("Raw")]
                        currentIndex: cppResponse.pretty ? 0 : 1
                        onActivated: (index) => cppResponse.pretty = index === 0
                    }

                    Label {
                        objectName: "truncatedLabel"

                        Layout.fillWidth: true
                        visible: cppResponse.truncated
                        elide: Text.ElideRight
                        opacity: 0.6
                        text: cppResponse.bodyOnDisk
                              ? qsTr("Showing the start of %1. Save the response to get all of it.").arg(cppResponse.sizeText)
                              : qsTr("Showing the first 1 MB.")
                    }

                    Item {
                        Layout.fillWidth: true
                        visible: !cppResponse.truncated
                    }
                }

                RowLayout {
                    /*! Topic of the F1 help. */
                    property string helpTopic: "response.find"

                    objectName: "findBar"

                    Layout.fillWidth: true
                    visible: root.findOpen
                    spacing: 2

                    TextField {
                        id: findField

                        /*! Topic of the F1 help. */
                        property string helpTopic: "response.find"

                        objectName: "findField"

                        Layout.fillWidth: true
                        placeholderText: qsTr("Find in response")
                        selectByMouse: true
                        onTextChanged: root.runFind()
                        Keys.onReturnPressed: (event) => root.moveFind(event.modifiers & Qt.ShiftModifier ? -1 : 1)
                        Keys.onEnterPressed: (event) => root.moveFind(event.modifiers & Qt.ShiftModifier ? -1 : 1)
                        Keys.onEscapePressed: root.closeFind()
                    }

                    Label {
                        objectName: "findCount"

                        opacity: 0.7
                        text: root.findCountText()
                    }

                    ToolButton {
                        /*! Topic of the F1 help. */
                        property string helpTopic: "response.find"

                        objectName: "findPrevious"

                        enabled: root.findMatches.length > 0
                        focusPolicy: Qt.NoFocus
                        text: "\u2191"
                        Accessible.name: qsTr("Previous match")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Previous match (Shift+F3)")
                        onClicked: root.moveFind(-1)
                    }

                    ToolButton {
                        /*! Topic of the F1 help. */
                        property string helpTopic: "response.find"

                        objectName: "findNext"

                        enabled: root.findMatches.length > 0
                        focusPolicy: Qt.NoFocus
                        text: "\u2193"
                        Accessible.name: qsTr("Next match")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Next match (F3)")
                        onClicked: root.moveFind(1)
                    }

                    ToolButton {
                        /*! Topic of the F1 help. */
                        property string helpTopic: "response.find"

                        objectName: "findClose"

                        focusPolicy: Qt.NoFocus
                        display: AbstractButton.IconOnly
                        icon.source: "qrc:/images/svg/close.svg"
                        icon.width: 16
                        icon.height: 16
                        Accessible.name: qsTr("Close find")
                        onClicked: root.closeFind()
                    }
                }

                Label {
                    objectName: "findShownPart"

                    Layout.fillWidth: true
                    visible: root.findOpen && cppResponse.truncated
                    wrapMode: Text.Wrap
                    opacity: 0.6
                    font.pixelSize: 11
                    text: qsTr("Only the shown part of the response is searched.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    Base.BsLineNumbers {
                        id: lineNumbers

                        objectName: "lineNumbers"

                        Layout.fillHeight: true
                        showNumbers: bodyArea.wrapMode === TextEdit.NoWrap
                        view: bodyScroll.contentItem
                        lineCount: cppResponse.bodyLineCount
                        textTopPadding: bodyArea.topPadding
                        font: bodyArea.font
                    }

                    ScrollView {
                        id: bodyScroll

                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        // Text and wrap mode are set here, not bound: separate
                        // bindings would lay out a large body twice. The empty text
                        // in between avoids that, and it colors a body in the new
                        // palette much faster than recoloring it in place.
                        TextArea {
                            id: bodyArea

                            /*! Topic of the F1 help. */
                            property string helpTopic: "response.body"

                            /*! Returns the wrap mode for the body that is shown now. */
                            function wrapModeForBody() {
                                if (cppResponse.pretty && cppResponse.canPretty)
                                    return TextEdit.NoWrap
                                return cppResponse.wrapAnywhere ? TextEdit.WrapAnywhere : TextEdit.Wrap
                            }

                            /*! Shows the current body; \a always also when it looks unchanged. */
                            function showBody(always) {
                                const body = cppResponse.bodyText
                                const wrap = bodyArea.wrapModeForBody()
                                if (!always && body === bodyArea.text && wrap === bodyArea.wrapMode)
                                    return
                                bodyArea.text = ""
                                bodyArea.wrapMode = wrap
                                bodyArea.text = body
                            }

                            /*! Measures the lines once the current layout pass is over. */
                            function measureLines() {
                                lineNumbers.lineHeight = bodyArea.contentHeight / cppResponse.bodyLineCount
                            }

                            objectName: "bodyArea"

                            readOnly: true
                            selectByMouse: true
                            // The match of a search stays marked while the find field has the focus.
                            persistentSelection: true
                            font.family: "Consolas"
                            onContentHeightChanged: Qt.callLater(bodyArea.measureLines)
                            Component.onCompleted: {
                                cppResponse.attachHighlighter(bodyArea.textDocument)
                                bodyArea.showBody(false)
                            }

                            Connections {
                                target: cppResponse

                                function onResponseChanged() {
                                    bodyArea.showBody(false)
                                    Qt.callLater(root.runFind)
                                }

                                function onDarkThemeChanged() {
                                    bodyArea.showBody(true)
                                }
                            }
                        }
                    }
                }
            }

            ListView {
                /*! Topic of the F1 help. */
                property string helpTopic: "response.headers"

                objectName: "headersList"

                clip: true
                model: cppResponse.headersModel

                ScrollBar.vertical: ScrollBar {}

                delegate: RowLayout {
                    id: headerRow

                    required property string key
                    required property string value

                    width: ListView.view.width
                    spacing: 8

                    Label {
                        objectName: "headerName"

                        Layout.preferredWidth: parent.width * 0.4
                        elide: Text.ElideRight
                        font.bold: true
                        textFormat: Text.PlainText
                        text: headerRow.key
                    }

                    Label {
                        objectName: "headerValue"

                        Layout.fillWidth: true
                        wrapMode: Text.WrapAnywhere
                        textFormat: Text.PlainText
                        text: headerRow.value
                    }
                }
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: cppResponse.hasResponse
        onActivated: root.openFind()
    }

    Shortcut {
        sequence: "F3"
        enabled: root.findOpen
        onActivated: root.moveFind(1)
    }

    Shortcut {
        sequence: "Shift+F3"
        enabled: root.findOpen
        onActivated: root.moveFind(-1)
    }

    FileDialog {
        id: saveDialog

        title: qsTr("Save Response")
        fileMode: FileDialog.SaveFile
        onAccepted: root.saveTo(selectedFile)
    }
}
