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

    /*! Ids of all selected nodes, the current one included; Ctrl and Shift add to it. */
    property var selectedIds: []

    /*! Emitted when the selection changes to \a id, which is empty for none. */
    signal nodeSelected(string id)

    /*! Emitted when a new item would be nested too deeply. */
    signal depthLimitReached()

    /*! Selects the node \a id and reports it. */
    function selectNode(id) {
        root.selectedIds = id.length > 0 ? [id] : []
        root.selectedId = id
        root.nodeSelected(id)
    }

    /*!
        Handles a click on the node \a id with \a modifiers: Ctrl adds or removes it,
        Shift selects the rows from the current node to it, and a plain click
        selects it alone.
    */
    function clickNode(id, modifiers) {
        if (modifiers & Qt.ControlModifier) {
            const ids = root.selectedIds.slice()
            const at = ids.indexOf(id)
            if (at >= 0)
                ids.splice(at, 1)
            else
                ids.push(id)
            root.selectedIds = ids
            // The current node is the one added last, or the last one left.
            const current = at < 0 ? id : (ids.length > 0 ? ids[ids.length - 1] : "")
            if (current !== root.selectedId) {
                root.selectedId = current
                root.nodeSelected(current)
            }
        } else if ((modifiers & Qt.ShiftModifier) && root.selectedId.length > 0) {
            const from = tree.rowAtIndex(cppCollection.indexOfNode(root.selectedId))
            const to = tree.rowAtIndex(cppCollection.indexOfNode(id))
            // An anchor hidden in a collapsed folder has no row: start from the clicked one.
            if (from < 0 || to < 0) {
                root.selectNode(id)
                return
            }
            const ids = []
            for (let row = Math.min(from, to); row <= Math.max(from, to); ++row)
                ids.push(cppCollection.idAt(tree.index(row, 0)))
            root.selectedIds = ids
        } else {
            root.selectNode(id)
        }
    }

    /*! Selects the visible row \a row and scrolls it into view. */
    function selectRow(row) {
        if (row < 0 || row >= tree.rows)
            return
        root.selectNode(cppCollection.idAt(tree.index(row, 0)))
        tree.positionViewAtRow(row, TableView.Contain)
    }

    /*!
        Handles \a event for the tree: arrows move and open folders, F2 renames,
        Delete deletes. Other keys are left to the tree.
    */
    function handleTreeKey(event) {
        const row = root.selectedId.length > 0 ? tree.rowAtIndex(cppCollection.indexOfNode(root.selectedId)) : -1
        switch (event.key) {
        case Qt.Key_Down:
            root.selectRow(row + 1)
            break
        case Qt.Key_Up:
            root.selectRow(Math.max(0, row - 1))
            break
        case Qt.Key_Right:
            if (row >= 0 && cppCollection.isFolder(root.selectedId))
                tree.expand(row)
            break
        case Qt.Key_Left:
            if (row >= 0 && tree.isExpanded(row))
                tree.collapse(row)
            break
        case Qt.Key_F2:
            if (row >= 0)
                root.openRename()
            break
        case Qt.Key_Delete:
            if (root.selectedIds.length > 0)
                root.openDelete()
            break
        default:
            return
        }
        event.accepted = true
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

    /*! Places a copy of each selected node after it and selects the copies. */
    function duplicateSelected() {
        if (root.selectedIds.length > 1) {
            const copies = cppCollection.duplicateItems(root.selectedIds)
            if (copies.length > 0) {
                root.selectNode(copies[0])
                root.selectedIds = copies
            }
            return
        }
        const id = cppCollection.duplicate(root.selectedId)
        if (id.length > 0) {
            root.selectNode(id)
            root.revealNode(id)
        }
    }

    /*! Sorts the folder of the selection, or the top level: folders first, then by name. */
    function sortSelectedFolder() {
        cppCollection.sortChildren(cppCollection.parentForNewItem(root.selectedId))
    }

    /*! Gives the selected node the name \a name. */
    function renameSelected(name) {
        cppCollection.rename(root.selectedId, name)
    }

    /*! Removes the selected nodes and everything below them. */
    function deleteSelected() {
        if (root.selectedIds.length > 1)
            cppCollection.removeItems(root.selectedIds)
        else
            cppCollection.remove(root.selectedId)
    }

    /*! Asks for a new name of the selected node. */
    function openRename() {
        renameField.text = cppCollection.nodeName(root.selectedId)
        renameDialog.open()
    }

    /*! Asks whether to delete the selected node, naming it as it is called now. */
    function openDelete() {
        if (root.selectedIds.length > 1) {
            deleteLabel.text = qsTr("Delete %n selected items and everything in them?", "", root.selectedIds.length)
            deleteDialog.open()
            return
        }
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
            root.selectedIds = root.selectedIds.filter(id => cppCollection.indexOfNode(id).valid)
            if (root.selectedId.length > 0 && !cppCollection.indexOfNode(root.selectedId).valid)
                root.selectNode("")
        }
    }

    TreeView {
        id: tree

        /*! Topic of the F1 help. */
        property string helpTopic: "collection.tree"

        objectName: "tree"

        anchors.fill: parent
        clip: true
        alternatingRows: false
        boundsBehavior: Flickable.StopAtBounds
        activeFocusOnTab: true
        model: cppCollection
        Accessible.name: qsTr("Collection")
        Keys.onPressed: (event) => root.handleTreeKey(event)

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
            implicitHeight: 30
            highlighted: nodeDelegate.nodeId === root.selectedId || root.selectedIds.indexOf(nodeDelegate.nodeId) >= 0
            Accessible.name: nodeDelegate.isFolder ? qsTr("Folder %1").arg(nodeDelegate.name)
                                                   : nodeDelegate.method + " " + nodeDelegate.name

            background: Rectangle {
                color: nodeDelegate.highlighted ? Base.BsTheme.selectedRowColor
                                                : nodeDelegate.hovered ? Qt.alpha(Base.BsTheme.accent, 0.06)
                                                                       : "transparent"

                Rectangle {
                    objectName: "selectionBar"

                    width: 3
                    height: parent.height
                    visible: nodeDelegate.highlighted
                    color: Base.BsTheme.accent
                }
            }

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
                    objectName: "nodeMethod"

                    Layout.preferredWidth: 42
                    visible: !nodeDelegate.isFolder
                    text: nodeDelegate.method
                    font.pixelSize: 10
                    font.bold: true
                    color: Base.BsTheme.methodColor(nodeDelegate.method)
                }

                Label {
                    objectName: "nodeName"

                    Layout.fillWidth: true
                    textFormat: Text.PlainText
                    text: nodeDelegate.name
                    elide: Text.ElideRight
                    color: Material.foreground
                }
            }

            // The left button goes through a mouse area, which reports the modifier
            // keys that the delegate's own clicked() does not.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPressed: tree.forceActiveFocus(Qt.MouseFocusReason)
                onClicked: (mouse) => root.clickNode(nodeDelegate.nodeId, mouse.modifiers)
            }

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
            text: qsTr("Sort A–Z")
            onTriggered: root.sortSelectedFolder()
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
        Material.roundedScale: Material.SmallScale
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
        Material.roundedScale: Material.SmallScale
        title: qsTr("Delete")
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: root.deleteSelected()

        Label {
            id: deleteLabel

            objectName: "deleteLabel"

            width: deleteDialog.availableWidth
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
    }
}
