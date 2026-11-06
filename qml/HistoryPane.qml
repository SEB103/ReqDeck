// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype HistoryPane
    \inqmlmodule ReqDeck
    \brief Lists the requests sent so far.

    History belongs to the application, not to the workspace. Double click or
    Return makes a new request of an entry in the collection, because the editor
    only shows collection requests. Literal credentials were left out when the
    entry was recorded, so they stay blank there.
*/
Item {
    id: root

    /*!
        Emitted after an entry became a new request.
        \a id is the id of that request in the collection.
    */
    signal entryOpened(string id)

    /*!
        Makes a new request of the entry at \a row.
        Taking the focus first stores a name or timeout still being edited.
    */
    function openRow(row) {
        historyList.forceActiveFocus(Qt.MouseFocusReason)
        const id = cppHistory.openEntry(row, cppCollection)
        if (id.length > 0)
            root.entryOpened(id)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        ListView {
            id: historyList

            objectName: "historyList"

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            activeFocusOnTab: true
            currentIndex: -1
            model: cppHistory
            Accessible.name: qsTr("History")
            Keys.onUpPressed: historyList.currentIndex = Math.max(0, historyList.currentIndex - 1)
            Keys.onDownPressed: historyList.currentIndex = Math.min(historyList.count - 1, historyList.currentIndex + 1)
            Keys.onReturnPressed: root.openRow(historyList.currentIndex)
            Keys.onEnterPressed: root.openRow(historyList.currentIndex)

            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: entry

                required property string method
                required property string url
                required property int statusCode
                required property double elapsedMs
                required property string errorString
                required property string timeText
                required property int index

                width: ListView.view.width
                highlighted: ListView.isCurrentItem
                Accessible.name: entry.method + " " + entry.url
                onPressed: {
                    historyList.forceActiveFocus(Qt.MouseFocusReason)
                    historyList.currentIndex = entry.index
                }
                onDoubleClicked: root.openRow(entry.index)

                contentItem: ColumnLayout {
                    spacing: 2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            objectName: "historyMethod"

                            text: entry.method
                            font.pixelSize: 10
                            font.bold: true
                            color: Base.BsTheme.accentTextColor
                        }

                        Label {
                            objectName: "historyUrl"

                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            text: entry.url
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            objectName: "historyResult"

                            Layout.fillWidth: true
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            opacity: 0.7
                            font.pixelSize: 11
                            text: entry.statusCode > 0
                                  ? qsTr("%1 · %2 ms").arg(entry.statusCode).arg(entry.elapsedMs)
                                  : entry.errorString
                        }

                        Label {
                            textFormat: Text.PlainText
                            opacity: 0.7
                            font.pixelSize: 11
                            text: entry.timeText
                        }
                    }
                }
            }

            Label {
                objectName: "emptyHint"

                parent: historyList
                anchors.centerIn: parent
                width: parent.width - 32
                visible: historyList.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                opacity: 0.6
                text: qsTr("Sent requests appear here.")
            }
        }

        Button {
            objectName: "clearHistory"

            flat: true
            enabled: historyList.count > 0
            text: qsTr("Clear history")
            onClicked: clearConfirmation.open()
        }
    }

    Dialog {
        id: clearConfirmation

        objectName: "clearConfirmation"

        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        title: qsTr("Clear history")
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: cppHistory.clear()

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr("Remove all entries from the history of all workspaces? This cannot be undone.")
        }
    }
}
