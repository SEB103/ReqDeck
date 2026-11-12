// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype CookieDialog
    \inqmlmodule ReqDeck
    \brief Shows the cookies of the workspace: edit a value, add, delete, or turn cookies off.

    Uses \c cppCookies. Changes apply at once; the cookies are kept apart from
    the workspace file, encrypted for the Windows user.
*/
Dialog {
    id: dialog

    /*! Topic of the F1 help. */
    property string helpTopic: "dialog.cookies"

    objectName: "cookieDialog"
    anchors.centerIn: Overlay.overlay
    width: Math.min(Overlay.overlay ? Overlay.overlay.width - 80 : 760, 760)
    height: Math.min(Overlay.overlay ? Overlay.overlay.height - 80 : 520, 520)
    modal: true
    Material.roundedScale: Material.SmallScale
    title: qsTr("Cookies")
    standardButtons: Dialog.Close

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Switch {
            objectName: "cookiesEnabled"

            text: qsTr("Send and store cookies in this workspace")
            checked: cppCookies.enabled
            Accessible.name: text
            onToggled: cppCookies.enabled = checked
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTr("Cookies a server sets are sent back to it with the next requests. A Cookie header typed in a request wins over them.")
        }

        Label {
            objectName: "noCookies"

            Layout.fillWidth: true
            visible: cppCookies.count === 0
            wrapMode: Text.Wrap
            opacity: 0.7
            text: qsTr("No cookies yet. They appear here when a response sets one.")
        }

        ListView {
            objectName: "cookieList"

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: cppCookies.count > 0
            clip: true
            model: cppCookies
            spacing: 2
            Accessible.name: qsTr("Cookies")

            ScrollBar.vertical: ScrollBar {}

            delegate: RowLayout {
                id: row

                required property int index
                required property string domain
                required property string name
                required property string value
                required property string path
                required property string expires
                required property bool secure

                width: ListView.view.width
                spacing: 8

                Label {
                    Layout.preferredWidth: 170
                    elide: Text.ElideMiddle
                    text: row.domain + (row.path === "/" ? "" : row.path)
                    ToolTip.visible: hovered.hovered
                    ToolTip.text: row.domain + row.path

                    HoverHandler {
                        id: hovered
                    }
                }

                Label {
                    Layout.preferredWidth: 120
                    elide: Text.ElideRight
                    font.bold: true
                    text: row.name
                }

                TextField {
                    objectName: "cookieValue"

                    Layout.fillWidth: true
                    selectByMouse: true
                    text: row.value
                    Accessible.name: qsTr("Value of %1").arg(row.name)
                    onEditingFinished: {
                        if (text !== row.value)
                            cppCookies.setValue(row.index, text)
                    }
                }

                Label {
                    Layout.preferredWidth: 130
                    elide: Text.ElideRight
                    opacity: 0.7
                    text: (row.expires.length > 0 ? row.expires : qsTr("this session")) + (row.secure ? " · HTTPS" : "")
                }

                ToolButton {
                    objectName: "deleteCookie"

                    icon.source: "qrc:/images/svg/close.svg"
                    Accessible.name: qsTr("Delete %1").arg(row.name)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Delete")
                    onClicked: cppCookies.remove(row.index)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            TextField {
                id: newDomain

                objectName: "newCookieDomain"

                Layout.preferredWidth: 170
                placeholderText: qsTr("Domain, for example api.example.com")
                selectByMouse: true
                Accessible.name: qsTr("Domain of the new cookie")
            }

            TextField {
                id: newName

                objectName: "newCookieName"

                Layout.preferredWidth: 120
                placeholderText: qsTr("Name")
                selectByMouse: true
                Accessible.name: qsTr("Name of the new cookie")
            }

            TextField {
                id: newValue

                objectName: "newCookieValue"

                Layout.fillWidth: true
                placeholderText: qsTr("Value")
                selectByMouse: true
                Accessible.name: qsTr("Value of the new cookie")
            }

            Button {
                objectName: "addCookie"

                text: qsTr("Add")
                enabled: newDomain.text.trim().length > 0 && newName.text.trim().length > 0
                onClicked: {
                    if (cppCookies.add(newDomain.text, newName.text, newValue.text, "/")) {
                        newName.clear()
                        newValue.clear()
                    }
                }
            }

            Button {
                objectName: "deleteAllCookies"

                text: qsTr("Delete All")
                enabled: cppCookies.count > 0
                onClicked: cppCookies.removeAll()
            }
        }
    }
}
