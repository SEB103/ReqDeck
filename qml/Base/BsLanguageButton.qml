// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

/*!
    \qmltype BsLanguageButton
    \inqmlmodule Base
    \brief Shows the interface language as letters such as EN and offers the others.

    A click opens a menu with every language, the active one checked. The host
    supplies the languages and the active one through properties and applies a
    choice from languageRequested(); the component reads no application context
    property. It shows itself only when there is more than one language.
*/
ToolButton {
    id: root

    /*! Topic of the F1 help. */
    property string helpTopic: "topbar.language"

    /*! Interface languages as {code, name, shortName} rows. */
    property var languages: []

    /*! Code of the active interface language, for example "de_DE". */
    property string currentLanguage: ""

    /*! Letters of the active interface language, for example "DE". */
    property string currentShortName: ""

    /*! Emitted when the user chooses the interface language \a code. */
    signal languageRequested(string code)

    objectName: "languageButton"

    visible: languages.length > 1
    text: currentShortName
    font.bold: true
    Accessible.name: qsTr("Interface language")
    ToolTip.visible: hovered && !languageMenu.visible
    ToolTip.text: qsTr("Interface language")
    onClicked: languageMenu.open()

    Menu {
        id: languageMenu

        objectName: "languageMenu"

        y: root.height

        Instantiator {
            model: root.languages

            delegate: MenuItem {
                required property var modelData

                text: modelData.name
                checkable: true
                checked: modelData.code === root.currentLanguage
                onTriggered: {
                    // A click toggles the check; the binding keeps it on the active language.
                    checked = Qt.binding(() => modelData.code === root.currentLanguage)
                    root.languageRequested(modelData.code)
                }
            }

            onObjectAdded: (index, object) => languageMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => languageMenu.removeItem(object)
        }
    }
}
