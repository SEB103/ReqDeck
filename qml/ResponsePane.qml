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
            text: qsTr("Sending %1…").arg(cppResponse.requestName)
        }

        Button {
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Base.BsStatusChip {
                objectName: "statusChip"

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

            Label {
                objectName: "requestNameLabel"

                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
                opacity: 0.6
                text: cppResponse.requestName
            }
        }

        Label {
            objectName: "errorLabel"

            Layout.fillWidth: true
            visible: cppResponse.errorText.length > 0
            wrapMode: Text.Wrap
            color: Material.color(Material.Red)
            text: cppResponse.errorText
        }

        Label {
            objectName: "warningLabel"

            Layout.fillWidth: true
            visible: cppResponse.warningText.length > 0
            wrapMode: Text.Wrap
            color: Material.color(Material.Orange)
            text: cppResponse.warningText
        }

        TabBar {
            id: responseTabs

            objectName: "responseTabs"

            Layout.fillWidth: true

            TabButton {
                text: qsTr("Body")
            }

            TabButton {
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

                    Switch {
                        objectName: "prettySwitch"

                        visible: cppResponse.canPretty
                        text: qsTr("Pretty")
                        checked: cppResponse.pretty
                        onToggled: cppResponse.pretty = checked
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

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    TextArea {
                        id: bodyArea

                        objectName: "bodyArea"

                        readOnly: true
                        selectByMouse: true
                        font.family: "Consolas"
                        wrapMode: cppResponse.pretty && cppResponse.canPretty ? TextEdit.NoWrap
                                                                              : TextEdit.Wrap
                        text: cppResponse.bodyText
                        Component.onCompleted: cppResponse.attachHighlighter(bodyArea.textDocument)
                    }
                }
            }

            ListView {
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
                        Layout.preferredWidth: parent.width * 0.4
                        elide: Text.ElideRight
                        font.bold: true
                        text: headerRow.key
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WrapAnywhere
                        text: headerRow.value
                    }
                }
            }
        }
    }
}
