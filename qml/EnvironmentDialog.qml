// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype EnvironmentDialog
    \inqmlmodule ReqDeck
    \brief Edits the environments of the workspace.

    Changes go straight into the workspace, so the dialog has Close and no Cancel.
    The environment shown here is independent of the active one. The name is
    stored when editing finishes, because the model trims names and refuses blank
    ones; every way of leaving the field stores it first.
*/
Dialog {
    id: root

    /*! Whether an environment is shown for editing. */
    readonly property bool hasEnvironment: cppEnvironments.editedRow >= 0

    /*! Shows the name of the edited environment. */
    function showName() {
        nameField.text = cppEnvironments.editedName
    }

    /*! Stores the name in the field; a refused name gives way to the stored one. */
    function commitName() {
        if (!root.hasEnvironment)
            return
        cppEnvironments.renameEnvironment(cppEnvironments.editedRow, nameField.text)
        root.showName()
    }

    /*! Moves the editor \a step rows through the list, stopping at its ends. */
    function moveEdited(step) {
        root.commitName()
        const row = cppEnvironments.editedRow + step
        if (row < 0 || row >= environmentList.count)
            return
        cppEnvironments.editedRow = row
        environmentList.positionViewAtIndex(row, ListView.Contain)
    }

    /*! Gets the name of a new environment ready to be replaced. */
    function focusName() {
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    objectName: "environmentDialog"

    anchors.centerIn: Overlay.overlay
    modal: true
    Material.roundedScale: Material.SmallScale
    title: qsTr("Environments")
    standardButtons: Dialog.Close
    closePolicy: Popup.CloseOnEscape

    onAboutToShow: {
        cppWorkspaceManager.retrySecrets()
        const first = cppEnvironments.names.length > 0 ? 0 : -1
        cppEnvironments.editedRow = cppEnvironments.activeIndex >= 0 ? cppEnvironments.activeIndex : first
        root.showName()
    }
    onAboutToHide: root.commitName()
    // Forgetting the row makes the next opening read the table again.
    onClosed: cppEnvironments.editedRow = -1

    Connections {
        target: cppEnvironments

        function onEditedNameChanged() {
            root.showName()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: false
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            spacing: 4

            ListView {
                id: environmentList

                /*! Topic of F1 help. */
                property string helpTopic: "environments.list"

                objectName: "environmentList"

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                activeFocusOnTab: true
                model: cppEnvironments
                Accessible.name: qsTr("Environments")
                Keys.onUpPressed: root.moveEdited(-1)
                Keys.onDownPressed: root.moveEdited(1)

                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: option

                    required property string name
                    required property bool active
                    required property int index

                    width: ListView.view.width
                    highlighted: cppEnvironments.editedRow === option.index
                    Accessible.name: option.name
                    onPressed: root.commitName()
                    onClicked: cppEnvironments.editedRow = option.index

                    contentItem: Label {
                        verticalAlignment: Text.AlignVCenter
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font.bold: option.active
                        text: option.name
                    }
                }

                Rectangle {
                    objectName: "listFocusFrame"

                    parent: environmentList
                    anchors.fill: parent
                    z: 1
                    visible: environmentList.activeFocus
                    color: "transparent"
                    border.color: Base.BsTheme.accent
                    border.width: 2
                }

                Label {
                    objectName: "emptyLabel"

                    parent: environmentList
                    anchors.centerIn: parent
                    width: parent.width - 16
                    visible: environmentList.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    opacity: 0.6
                    text: qsTr("No environments yet.")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 2

                ToolButton {
                    objectName: "addEnvironment"

                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/images/svg/note_add.svg"
                    icon.width: 20
                    icon.height: 20
                    Accessible.name: qsTr("Add environment")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Add environment")
                    onClicked: {
                        root.commitName()
                        cppEnvironments.addEnvironment()
                        root.focusName()
                    }
                }

                ToolButton {
                    objectName: "duplicateEnvironment"

                    enabled: root.hasEnvironment && !cppWorkspaceManager.secretsUnread
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/images/svg/content_copy.svg"
                    icon.width: 20
                    icon.height: 20
                    Accessible.name: qsTr("Duplicate environment")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Duplicate environment")
                    onClicked: {
                        root.commitName()
                        cppEnvironments.duplicateEnvironment(cppEnvironments.editedRow)
                        root.focusName()
                    }
                }

                ToolButton {
                    objectName: "removeEnvironment"

                    enabled: root.hasEnvironment
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/images/svg/close.svg"
                    icon.width: 20
                    icon.height: 20
                    Accessible.name: qsTr("Remove environment")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Remove environment")
                    onClicked: {
                        root.commitName()
                        removeConfirmation.open()
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            TextField {
                id: nameField

                objectName: "environmentName"

                Layout.fillWidth: true
                enabled: root.hasEnvironment
                placeholderText: qsTr("Environment name")
                selectByMouse: true
                onEditingFinished: root.commitName()
            }

            Label {
                objectName: "secretsNote"

                Layout.fillWidth: true
                visible: cppWorkspaceManager.secretsUnread
                wrapMode: Text.Wrap
                text: qsTr("The secret values could not be read, so variables cannot be changed now. Open this dialog again to retry.")
            }

            // Stored secrets are matched by key and position, so editing rows
            // before they are read would lose them or give them to the wrong row.
            Base.BsKeyValueTable {
                objectName: "variablesTable"

                Layout.fillWidth: true
                Layout.fillHeight: true
                enabled: root.hasEnvironment && !cppWorkspaceManager.secretsUnread
                showSecret: true
                keyTitle: qsTr("Variable")
                emptyHint: qsTr("Add a variable to use it as {{name}} in requests.")
                model: cppEnvironments.variablesModel
            }
        }
    }

    Dialog {
        id: removeConfirmation

        objectName: "removeConfirmation"

        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        Material.roundedScale: Material.SmallScale
        title: qsTr("Remove environment")
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: cppEnvironments.removeEnvironment(cppEnvironments.editedRow)

        Label {
            objectName: "removeLabel"

            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: qsTr("Remove the environment \"%1\"? This cannot be undone.")
                      .arg(cppEnvironments.editedName)
        }
    }
}
