// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
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
                        text: qsTr("Showing the first 1 MB.")
                    }

                    Item {
                        Layout.fillWidth: true
                        visible: !cppResponse.truncated
                    }
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
}
