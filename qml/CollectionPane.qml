// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype CollectionPane
    \inqmlmodule ReqDeck
    \brief Shows the collection as a tree and edits its structure.

    Reads the \c cppCollection model. New items go into the selected folder, or
    next to the selected request, and become the selection.
*/
Item {
    id: root

    /*! Id of the selected node; empty when nothing is selected. */
    property string selectedId: ""

    /*! Emitted when the selection changes to \a id, which is empty for none. */
    signal nodeSelected(string id)

    /*! Emitted when a new item would be nested too deeply. */
    signal depthLimitReached()

    /*! Selects the node \a id and reports it. */
    function selectNode(id) {
        root.selectedId = id
        root.nodeSelected(id)
    }

    /*! Expands the parents of the node \a id and scrolls it into view. */
    function revealNode(id) {
        const index = cppCollection.indexOfNode(id)
        tree.expandToIndex(index)
        tree.forceLayout()
        tree.positionViewAtRow(tree.rowAtIndex(index), TableView.Contain)
    }

    /*! Selects and shows the new node \a id, or reports that it was refused. */
    function finishAdding(id) {
        if (id.length === 0) {
            root.depthLimitReached()
            return
        }
        root.selectNode(id)
        root.revealNode(id)
    }

    /*! Adds a request at the place given by the selection. */
    function addRequest() {
        root.finishAdding(cppCollection.addRequest(cppCollection.parentForNewItem(root.selectedId)))
    }

    /*! Adds a folder at the place given by the selection. */
    function addFolder() {
        root.finishAdding(cppCollection.addFolder(cppCollection.parentForNewItem(root.selectedId)))
    }

    /*! Places a copy of the selected node after it and selects the copy. */
    function duplicateSelected() {
        const id = cppCollection.duplicate(root.selectedId)
        if (id.length > 0) {
            root.selectNode(id)
            root.revealNode(id)
        }
    }

    /*! Gives the selected node the name \a name. */
    function renameSelected(name) {
        cppCollection.rename(root.selectedId, name)
    }

    /*! Removes the selected node and everything below it. */
    function deleteSelected() {
        cppCollection.remove(root.selectedId)
    }

    /*! Asks for a new name of the selected node. */
    function openRename() {
        renameField.text = cppCollection.nodeName(root.selectedId)
        renameDialog.open()
    }

    /*! Asks whether to delete the selected node, naming it as it is called now. */
    function openDelete() {
        const name = cppCollection.nodeName(root.selectedId)
        deleteLabel.text = cppCollection.isFolder(root.selectedId)
                           ? qsTr("Delete folder \"%1\" and everything in it?").arg(name)
                           : qsTr("Delete request \"%1\"?").arg(name)
        deleteDialog.open()
    }

    // The selection goes with its node, also when a whole folder or the
    // workspace disappears.
    Connections {
        target: cppCollection

        function onCollectionChanged() {
            if (root.selectedId.length > 0 && !cppCollection.indexOfNode(root.selectedId).valid)
                root.selectNode("")
        }
    }

    TreeView {
        id: tree

        objectName: "tree"

        anchors.fill: parent
        clip: true
        alternatingRows: false
        boundsBehavior: Flickable.StopAtBounds
        model: cppCollection

        ScrollBar.vertical: ScrollBar {}

        // A press takes the focus first: a field in the request pane must commit
        // its text before the selection loads another request.
        TapHandler {
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onTapped: (eventPoint, button) => {
                const position = tree.contentItem.mapFromItem(tree, eventPoint.position)
                if (tree.cellAtPosition(position).y >= 0)
                    return
                tree.forceActiveFocus(Qt.MouseFocusReason)
                root.selectNode("")
                if (button === Qt.RightButton)
                    nodeMenu.popup()
            }
        }

        delegate: TreeViewDelegate {
            id: nodeDelegate

            required property string nodeId
            required property string name
            required property string method
            required property bool isFolder

            implicitWidth: tree.width
            highlighted: nodeDelegate.nodeId === root.selectedId

            contentItem: RowLayout {
                spacing: 6

                ToolButton {
                    visible: nodeDelegate.isFolder
                    enabled: false
                    padding: 0
                    background: null
                    display: AbstractButton.IconOnly
                    icon.source: "qrc:/images/svg/folder.svg"
                    icon.width: 16
                    icon.height: 16
                    icon.color: Material.foreground
                }

                Label {
                    visible: !nodeDelegate.isFolder
                    text: nodeDelegate.method
                    font.pixelSize: 10
                    font.bold: true
                    color: Base.BsTheme.accentTextColor
                }

                Label {
                    Layout.fillWidth: true
                    text: nodeDelegate.name
                    elide: Text.ElideRight
                }
            }

            onPressed: tree.forceActiveFocus(Qt.MouseFocusReason)
            onClicked: root.selectNode(nodeDelegate.nodeId)

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: {
                    tree.forceActiveFocus(Qt.MouseFocusReason)
                    root.selectNode(nodeDelegate.nodeId)
                    nodeMenu.popup()
                }
            }
        }
    }

    Label {
        objectName: "emptyHint"

        anchors.centerIn: parent
        width: parent.width - 32
        visible: tree.rows === 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        opacity: 0.6
        text: qsTr("No requests yet. Use Request > New Request to add one.")
    }

    Menu {
        id: nodeMenu

        objectName: "nodeMenu"

        delegate: Base.BsMenuItem {}

        Base.BsMenuItem {
            text: qsTr("New Request")
            icon.source: "qrc:/images/svg/note_add.svg"
            onTriggered: root.addRequest()
        }

        Base.BsMenuItem {
            text: qsTr("New Folder")
            icon.source: "qrc:/images/svg/folder.svg"
            onTriggered: root.addFolder()
        }

        MenuSeparator {}

        Base.BsMenuItem {
            text: qsTr("Rename…")
            enabled: root.selectedId.length > 0
            onTriggered: root.openRename()
        }

        Base.BsMenuItem {
            text: qsTr("Duplicate")
            icon.source: "qrc:/images/svg/content_copy.svg"
            enabled: root.selectedId.length > 0
            onTriggered: root.duplicateSelected()
        }

        Base.BsMenuItem {
            text: qsTr("Delete…")
            icon.source: "qrc:/images/svg/close.svg"
            enabled: root.selectedId.length > 0
            onTriggered: root.openDelete()
        }
    }

    Dialog {
        id: renameDialog

        objectName: "renameDialog"

        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("Rename")
        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: root.renameSelected(renameField.text)
        onOpened: {
            renameField.forceActiveFocus()
            renameField.selectAll()
        }

        TextField {
            id: renameField

            /*! Whether the text can become a name. */
            readonly property bool valid: text.trim().length > 0

            objectName: "renameField"

            implicitWidth: 320
            selectByMouse: true
            onAccepted: {
                if (valid)
                    renameDialog.accept()
            }
            onValidChanged: renameDialog.standardButton(Dialog.Ok).enabled = valid
        }
    }

    Dialog {
        id: deleteDialog

        objectName: "deleteDialog"

        anchors.centerIn: Overlay.overlay
        width: 420
        modal: true
        title: qsTr("Delete")
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: root.deleteSelected()

        Label {
            id: deleteLabel

            objectName: "deleteLabel"

            width: deleteDialog.availableWidth
            wrapMode: Text.Wrap
        }
    }
}
