// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype AboutDialog
    \inqmlmodule ReqDeck
    \brief Shows application, build, and licensing information.

    The dialog presents the application identity and version, the toolchain and
    third-party component versions, the application's own GPL-3.0-or-later
    license, and the third-party notices and full license texts. Application
    metadata comes from the \c cppAppInfo context object; the list of bundled
    license documents comes from \c cppLicenseModel.
*/
Dialog {
    id: root

    /*! Whether the surrounding window uses the dark Material theme. */
    property bool darkTheme: false

    /*! Index of the license selected in the Licenses tab. */
    property int selectedLicense: 0

    title: qsTr("About %1").arg(cppAppInfo.appName)
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(root.parent ? root.parent.width - 80 : 900, 900)
    height: Math.min(root.parent ? root.parent.height - 80 : 660, 680)
    standardButtons: Dialog.Close
    closePolicy: Popup.CloseOnEscape

    Material.accent: Base.BsTheme.accent

    header: TabBar {
        id: tabBar

        TabButton { text: qsTr("About") }
        TabButton { text: qsTr("License") }
        TabButton { text: qsTr("Third-Party") }
        TabButton { text: qsTr("Licenses") }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: tabBar.currentIndex

        // --- About -----------------------------------------------------------
        ColumnLayout {
            spacing: 16

            // Identity band: logo plus name, version, license and copyright,
            // tinted with the application accent color.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 148
                radius: 6
                color: Qt.alpha(Material.accentColor, root.darkTheme ? 0.18 : 0.12)

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 16

                    Image {
                        // Application logo, rendered large and crisp (high
                        // sourceSize + mipmap).
                        source: "qrc:/images/app/ReqDeckLogo.png"
                        sourceSize.width: 256
                        sourceSize.height: 256
                        Layout.preferredWidth: 116
                        Layout.preferredHeight: 116
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: cppAppInfo.appName
                            font.pixelSize: 26
                            font.bold: true
                        }
                        Label {
                            text: qsTr("Version %1").arg(cppAppInfo.appVersion)
                            opacity: 0.9
                        }
                        Label {
                            text: cppAppInfo.copyright
                            opacity: 0.75
                            font.pixelSize: 12
                        }
                        Label {
                            text: qsTr("License: %1").arg(cppAppInfo.licenseName)
                            opacity: 0.75
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: cppAppInfo.description
                wrapMode: Text.Wrap
                opacity: 0.9
            }

            // Build and environment details.
            GridLayout {
                columns: 2
                columnSpacing: 24
                rowSpacing: 6

                Label { text: qsTr("Qt:"); font.bold: true }
                Label { text: cppAppInfo.qtVersion }

                Label { text: qsTr("Compiler:"); font.bold: true }
                Label { text: cppAppInfo.compiler }

                Label { text: qsTr("C++ standard:"); font.bold: true }
                Label { text: cppAppInfo.cxxStandard }

                Label { text: qsTr("Build:"); font.bold: true }
                Label { text: "%1 · %2".arg(cppAppInfo.buildType).arg(cppAppInfo.buildTimestamp) }

                Label { text: qsTr("OS:"); font.bold: true }
                Label { text: cppAppInfo.operatingSystem }

                Label { text: qsTr("CPU:"); font.bold: true }
                Label { text: cppAppInfo.cpuArchitecture }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Homepage: <a href=\"%1\">%1</a>").arg(cppAppInfo.homepageUrl)
                textFormat: Text.RichText
                onLinkActivated: (link) => Qt.openUrlExternally(link)

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            Item { Layout.fillHeight: true }
        }

        // --- License (this application) --------------------------------------
        ColumnLayout {
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("License of this application (%1). This applies to the %2 source code itself.")
                          .arg(cppAppInfo.licenseName)
                          .arg(cppAppInfo.appName)
                wrapMode: Text.Wrap
                font.bold: true
            }

            ScrollView {
                id: licenseScroll

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                TextArea {
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    font.family: "monospace"
                    text: cppAppInfo.readText(cppAppInfo.licensesRoot + "/LICENSES/GPL-3.0-or-later.txt")
                }
            }
        }

        // --- Third-Party notices ---------------------------------------------
        ColumnLayout {
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: qsTr("Third-party components. These licenses apply to bundled or dynamically linked components, not to the %1 source code.")
                          .arg(cppAppInfo.appName)
                wrapMode: Text.Wrap
                font.bold: true
            }

            ScrollView {
                id: thirdPartyScroll

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                TextArea {
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    text: cppAppInfo.readText(cppAppInfo.licensesRoot + "/THIRD_PARTY_NOTICES.md")
                }
            }
        }

        // --- Full third-party license texts ----------------------------------
        RowLayout {
            spacing: 12

            ListView {
                id: licenseList

                Layout.preferredWidth: 190
                Layout.fillHeight: true
                clip: true
                model: cppLicenseModel
                currentIndex: root.selectedLicense
                onCurrentIndexChanged: root.selectedLicense = currentIndex

                delegate: ItemDelegate {
                    id: licenseDelegate

                    required property int index
                    required property string title

                    width: ListView.view.width
                    text: licenseDelegate.title
                    highlighted: ListView.isCurrentItem
                    onClicked: licenseList.currentIndex = licenseDelegate.index
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                opacity: 0.25
                color: Material.foreground
            }

            ScrollView {
                id: fullLicenseScroll

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth

                TextArea {
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    font.family: "monospace"
                    text: {
                        const path = cppLicenseModel.pathAt(root.selectedLicense)
                        return path ? cppAppInfo.readText(path) : qsTr("No license selected.")
                    }
                }
            }
        }
    }
}
