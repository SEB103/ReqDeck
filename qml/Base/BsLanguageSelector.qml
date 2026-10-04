// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

/*!
    \qmltype BsLanguageSelector
    \inqmlmodule Base
    \brief UI-language selector combo box showing each language's flag and endonym.

    Bound to the \c cppLocale controller: it lists \c cppLocale.availableLanguages
    (flag + name), shows the active language with its flag when closed, applies the
    selection live via \c cppLocale.setLanguage(), and selects the active language
    from \c cppLocale.currentLanguage once its model is ready — so the pre-selected
    language is visible immediately (a \c currentIndex binding to \c indexOfValue()
    can evaluate before the model is populated and leave the field blank on first
    launch).
*/
ComboBox {
    id: selector

    /*! Fixed on-screen size of every flag image, so all locales occupy an
        identical box regardless of each SVG's internal geometry. The 24x18 box
        is an exact 4:3 ratio, so the flags fill it without distortion. */
    readonly property int flagWidth: 24
    readonly property int flagHeight: 18

    model: cppLocale.availableLanguages
    textRole: "name"
    valueRole: "code"

    Accessible.name: qsTr("Interface language")

    // Apply the chosen language live to the whole UI.
    onActivated: cppLocale.setLanguage(currentValue)

    // Select the active language after the model is populated, then keep the row in
    // sync with any external language change.
    Component.onCompleted: currentIndex = indexOfValue(cppLocale.currentLanguage)

    Connections {
        target: cppLocale
        function onCurrentLanguageChanged() {
            selector.currentIndex = selector.indexOfValue(cppLocale.currentLanguage)
        }
    }

    // Closed field: flag of the active language followed by its name.
    contentItem: RowLayout {
        spacing: 8

        Image {
            source: cppLocale.currentFlag
            sourceSize.width: selector.flagWidth
            sourceSize.height: selector.flagHeight
            fillMode: Image.Stretch
            Layout.leftMargin: 10
            Layout.preferredWidth: selector.flagWidth
            Layout.preferredHeight: selector.flagHeight
            Layout.alignment: Qt.AlignVCenter
        }

        Label {
            text: selector.displayText
            color: Material.foreground
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true
        }
    }

    // Dropdown rows: each language's flag followed by its name.
    delegate: ItemDelegate {
        id: languageItem

        required property var modelData
        required property int index

        width: ListView.view ? ListView.view.width : implicitWidth
        highlighted: selector.highlightedIndex === languageItem.index

        contentItem: RowLayout {
            spacing: 8

            Image {
                source: languageItem.modelData.flag
                sourceSize.width: selector.flagWidth
                sourceSize.height: selector.flagHeight
                fillMode: Image.Stretch
                Layout.preferredWidth: selector.flagWidth
                Layout.preferredHeight: selector.flagHeight
                Layout.alignment: Qt.AlignVCenter
            }

            Label {
                text: languageItem.modelData.name
                color: Material.foreground
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
            }
        }
    }
}
