// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype UpdateDialog
    \inqmlmodule ReqDeck
    \brief Shows the result of an application update check.

    The dialog reflects the \c cppUpdate controller: it shows a busy indicator
    while a check runs, reports whether the running version is current, and offers
    the matching action when a newer version is available: an installed copy can
    start the Maintenance Tool (\l installUpdateRequested, after which the
    application closes), a portable copy or a build without a Maintenance Tool
    opens the download page. When the update feature is disabled in the product
    configuration it explains that the check is not yet available.
*/
Dialog {
    id: root

    /*! Whether the surrounding window uses the dark Material theme. */
    property bool darkTheme: false

    /*!
        Emitted when the user asks to install the available update through the
        Maintenance Tool. The owner runs the unsaved-changes guards and then calls
        \c cppUpdate.installUpdate(); the dialog never starts the tool itself.
    */
    signal installUpdateRequested()

    title: qsTr("Check for updates")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(root.parent ? root.parent.width - 80 : 460, 460)
    standardButtons: Dialog.Close
    closePolicy: Popup.CloseOnEscape

    Material.accent: Base.BsTheme.accent

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        Label {
            Layout.fillWidth: true
            text: qsTr("Current version: %1").arg(cppUpdate.currentVersion)
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            BusyIndicator {
                running: cppUpdate.checking
                visible: running
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
            }

            Label {
                Layout.fillWidth: true
                text: cppUpdate.statusMessage
                wrapMode: Text.Wrap
            }
        }

        // Installed copies update in place; the application closes first so the
        // Maintenance Tool can replace its files.
        Label {
            Layout.fillWidth: true
            visible: cppUpdate.canInstallUpdate
            text: qsTr("The application will close before the update is installed.")
            wrapMode: Text.Wrap
        }

        // Portable copies and development builds download the release manually.
        Label {
            Layout.fillWidth: true
            visible: cppUpdate.updateAvailable && !cppUpdate.canInstallUpdate
            text: qsTr("Download the new version from the release page and replace this copy manually.")
            wrapMode: Text.Wrap
        }

        // Offer the matching action only when a newer version was found.
        Button {
            Layout.alignment: Qt.AlignLeft
            visible: cppUpdate.updateAvailable
            text: cppUpdate.canInstallUpdate ? qsTr("Install update") : qsTr("Open download page")
            onClicked: {
                if (cppUpdate.canInstallUpdate)
                    root.installUpdateRequested()
                else
                    cppUpdate.openDownloadPage()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: cppUpdate.actionError !== ""
            text: cppUpdate.actionError
            color: Material.color(Material.Red)
            wrapMode: Text.Wrap
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Check again")
            enabled: cppUpdate.featureEnabled && !cppUpdate.checking
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: cppUpdate.checkNow()
        }

        Button {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.reject()
        }
    }
}
