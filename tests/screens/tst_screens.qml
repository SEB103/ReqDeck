// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtTest
import ReqDeck

/*!
    \qmltype tst_screens
    \brief Runs the collection and request panes against the real facades.

    The panes read the application's context, so tst_screens.cpp publishes real
    objects over a temporary workspace holding the collection f1(r1) and r2.
*/
Item {
    id: root

    width: 1000
    height: 700

    Component {
        id: collectionComponent

        CollectionPane {
            width: 300
            height: 600
        }
    }

    Component {
        id: requestComponent

        RequestPane {
            width: 700
            height: 600
        }
    }

    // Connected like Main.qml connects the two panes.
    Component {
        id: wiredComponent

        Item {
            property alias collection: collectionPane
            property alias request: requestPane

            width: 1000
            height: 600

            CollectionPane {
                id: collectionPane

                width: 300
                height: 600
                onNodeSelected: (id) => cppRequestEditor.load(id)
            }

            RequestPane {
                id: requestPane

                x: 300
                width: 700
                height: 600
            }
        }
    }

    SignalSpy {
        id: selectedSpy

        signalName: "nodeSelected"
    }

    SignalSpy {
        id: limitSpy

        signalName: "depthLimitReached"
    }

    TestCase {
        id: testCase

        /*! Exercises the panes with the application's facades. */
        name: "Screens"
        when: windowShown

        function init() {
            verify(cppTestHost.reset());
            selectedSpy.clear();
            limitSpy.clear();
        }

        /*! Creates the collection pane and waits until its tree is laid out. */
        function createCollection() {
            const pane = createTemporaryObject(collectionComponent, root);
            verify(pane !== null);
            tryCompare(findChild(pane, "tree"), "rows", 2);
            return pane;
        }

        /*! Creates both panes wired together, with the tree laid out. */
        function createWired() {
            const host = createTemporaryObject(wiredComponent, root);
            verify(host !== null);
            tryCompare(findChild(host.collection, "tree"), "rows", 2);
            waitForRendering(host);
            return host;
        }

        /*! Clicks \a row of the tree in \a host, with \a button if given. */
        function clickRow(host, row, button) {
            const tree = findChild(host.collection, "tree");
            const item = tree.itemAtCell(0, row);
            mouseClick(item, item.width / 2, item.height / 2, button === undefined ? Qt.LeftButton : button);
            wait(50);
        }

        /*! Types one letter at the end of \a field. */
        function typeInto(field) {
            field.forceActiveFocus();
            field.cursorPosition = field.length;
            keyClick(Qt.Key_X);
        }

        /*! Creates the request pane showing the request \a id. */
        function createRequest(id) {
            const pane = createTemporaryObject(requestComponent, root);
            verify(pane !== null);
            cppRequestEditor.load(id);
            waitForRendering(pane);
            return pane;
        }

        /*! Verifies that the tree shows the top level with folders collapsed. */
        function test_collectionShowsTopLevel() {
            const tree = findChild(createCollection(), "tree");
            compare(tree.rows, 2);
            verify(tree.itemAtCell(0, 0) !== null);
        }

        /*! Verifies that clicking a row selects its node. */
        function test_clickSelectsNode() {
            const pane = createCollection();
            selectedSpy.target = pane;
            const tree = findChild(pane, "tree");

            mouseClick(tree.itemAtCell(0, 1));

            compare(pane.selectedId, "r2");
            compare(selectedSpy.count, 1);
            compare(selectedSpy.signalArguments[0][0], "r2");
        }

        /*! Verifies that a new item at the top level is selected. */
        function test_addRequestSelectsNewNode() {
            const pane = createCollection();
            selectedSpy.target = pane;

            pane.addRequest();

            tryCompare(findChild(pane, "tree"), "rows", 3);
            verify(pane.selectedId.length > 0);
            compare(selectedSpy.count, 1);
            verify(cppWorkspaceManager.dirty);
        }

        /*! Verifies that a new item in a folder expands that folder. */
        function test_addInsideFolderExpandsIt() {
            const pane = createCollection();
            pane.selectedId = "f1";

            pane.addFolder();

            // f1, r1, the new folder, r2.
            tryCompare(findChild(pane, "tree"), "rows", 4);
        }

        /*! Verifies that the selection goes when its node is removed. */
        function test_removedNodeClearsSelection() {
            const pane = createCollection();
            selectedSpy.target = pane;
            pane.selectedId = "r2";

            verify(cppCollection.remove("r2"));

            compare(pane.selectedId, "");
            compare(selectedSpy.count, 1);
            compare(selectedSpy.signalArguments[0][0], "");
        }

        /*! Verifies renaming and deleting the selected node. */
        function test_renameAndDeleteSelected() {
            const pane = createCollection();
            pane.selectedId = "r2";

            pane.renameSelected("Renamed");
            compare(cppCollection.nodeName("r2"), "Renamed");
            verify(cppWorkspaceManager.dirty);

            pane.deleteSelected();
            verify(!cppCollection.indexOfNode("r2").valid);
        }

        /*! Verifies that the selected node can be duplicated. */
        function test_duplicateSelectsCopy() {
            const pane = createCollection();
            pane.selectedId = "r2";

            pane.duplicateSelected();

            tryCompare(findChild(pane, "tree"), "rows", 3);
            verify(pane.selectedId !== "r2");
        }

        /*! Verifies that the depth limit is reported, not silently swallowed. */
        function test_depthLimitIsReported() {
            const pane = createCollection();
            limitSpy.target = pane;

            let parentId = "";
            for (let level = 0; level < 64; ++level)
                parentId = cppCollection.addFolder(parentId);

            pane.selectedId = parentId;
            pane.addFolder();
            compare(limitSpy.count, 1);
        }

        /*! Verifies the placeholder while no request is open. */
        function test_requestPaneWithoutRequest() {
            const pane = createTemporaryObject(requestComponent, root);
            verify(pane !== null);
            compare(findChild(pane, "editor").visible, false);
            compare(findChild(pane, "placeholder").visible, true);
        }

        /*! Verifies that an opened request fills the fields and stays clean. */
        function test_requestPaneShowsLoadedRequest() {
            const pane = createRequest("r1");

            compare(findChild(pane, "editor").visible, true);
            compare(findChild(pane, "nameField").text, "Create item");
            compare(findChild(pane, "urlField").text, "http://h/items");
            compare(findChild(pane, "methodCombo").currentText, "POST");
            compare(findChild(pane, "sendButton").enabled, false);
            verify(!cppWorkspaceManager.dirty);
        }

        /*! Verifies that typing in the URL field reaches the request. */
        function test_typingUrlWritesThrough() {
            const pane = createRequest("r1");
            const field = findChild(pane, "urlField");

            field.forceActiveFocus();
            field.cursorPosition = field.length;
            keyClick(Qt.Key_X);

            compare(cppRequestEditor.url, "http://h/itemsx");
            verify(cppWorkspaceManager.dirty);
        }

        /*! Verifies that the fields follow another request without writing. */
        function test_switchingRequestUpdatesFields() {
            const pane = createRequest("r1");

            cppRequestEditor.load("r2");
            waitForRendering(pane);

            compare(findChild(pane, "urlField").text, "");
            compare(findChild(pane, "methodCombo").currentText, "GET");
            verify(!cppWorkspaceManager.dirty);
        }

        /*! Verifies that choosing a method reaches the request. */
        function test_methodComboWritesThrough() {
            const pane = createRequest("r1");
            const combo = findChild(pane, "methodCombo");

            combo.forceActiveFocus();
            keyClick(Qt.Key_Down);
            compare(cppRequestEditor.method, "PUT");

            // The binding must survive the user's choice.
            cppRequestEditor.load("r2");
            waitForRendering(pane);
            compare(combo.currentText, "GET");
        }

        /*! Verifies that a name being typed survives a click on another row. */
        function test_typedNameSurvivesClickOnRow() {
            const host = createWired();
            clickRow(host, 1);
            compare(cppRequestEditor.requestId, "r2");

            typeInto(findChild(host.request, "nameField"));
            clickRow(host, 0);

            compare(cppCollection.nodeName("r2"), "List itemsx");
            verify(cppWorkspaceManager.dirty);
        }

        /*! Verifies that a name being typed survives a click on its own row. */
        function test_typedNameSurvivesClickOnSameRow() {
            const host = createWired();
            clickRow(host, 1);

            typeInto(findChild(host.request, "nameField"));
            clickRow(host, 1);

            compare(cppCollection.nodeName("r2"), "List itemsx");
            compare(findChild(host.request, "nameField").text, "List itemsx");
        }

        /*! Verifies that a name being typed survives opening the context menu. */
        function test_typedNameSurvivesRightClick() {
            const host = createWired();
            clickRow(host, 1);

            typeInto(findChild(host.request, "nameField"));
            clickRow(host, 0, Qt.RightButton);

            compare(cppCollection.nodeName("r2"), "List itemsx");
            compare(host.collection.selectedId, "f1");
            findChild(host.collection, "nodeMenu").close();
        }

        /*! Verifies that opening a request with CRLF text changes nothing. */
        function test_loadWithFocusInBodyKeepsRequestClean() {
            const host = createWired();
            const area = findChild(host.request, "rawBodyArea");
            cppRequestEditor.load("r2");
            cppRequestEditor.rawBody = "a\r\nb";
            verify(cppWorkspaceManager.saveWorkspace());
            verify(!cppWorkspaceManager.dirty);

            cppRequestEditor.load("r1");
            findChild(host.request, "tabs").currentIndex = 2;
            area.forceActiveFocus();
            verify(area.activeFocus);

            cppRequestEditor.load("r2");
            waitForRendering(host);

            compare(cppRequestEditor.rawBody, "a\r\nb");
            verify(!cppWorkspaceManager.dirty);
        }

        /*! Verifies that typing in the body keeps the cursor in place. */
        function test_typingRawBodyKeepsCursor() {
            const pane = createRequest("r1");
            findChild(pane, "tabs").currentIndex = 2;
            const area = findChild(pane, "rawBodyArea");

            area.forceActiveFocus();
            area.cursorPosition = 1;
            keyClick(Qt.Key_X);

            compare(cppRequestEditor.rawBody, "{x}");
            compare(area.cursorPosition, 2);
        }

        /*! Verifies that a click below the rows clears the selection. */
        function test_clickOnEmptySpaceClearsSelection() {
            const host = createWired();
            clickRow(host, 1);
            compare(host.collection.selectedId, "r2");

            const tree = findChild(host.collection, "tree");
            mouseClick(tree, tree.width / 2, tree.height - 10);

            compare(host.collection.selectedId, "");
            compare(cppRequestEditor.hasRequest, false);
        }

        /*! Verifies that a new item can reach the top level after a selection. */
        function test_newItemAfterClearingGoesToTopLevel() {
            const host = createWired();
            clickRow(host, 0);
            const tree = findChild(host.collection, "tree");
            mouseClick(tree, tree.width / 2, tree.height - 10);

            host.collection.addFolder();

            tryCompare(tree, "rows", 3);
            compare(cppCollection.rowCount(), 3);
        }

        /*! Verifies that a click on a row is not taken for empty space. */
        function test_rowClicksKeepSelection() {
            const host = createWired();
            clickRow(host, 1);
            clickRow(host, 0, Qt.RightButton);
            compare(host.collection.selectedId, "f1");
            findChild(host.collection, "nodeMenu").close();
        }

        /*! Verifies that the tabs use the full width after changes while hidden. */
        function test_tabsFillWidthAfterHiddenChange() {
            const pane = createRequest("r2");
            const stack = findChild(pane, "tabStack");
            waitForRendering(pane);

            cppRequestEditor.bodyMode = 1; // BodyMode::Raw
            cppRequestEditor.authType = 1; // AuthType::Basic
            findChild(pane, "tabs").currentIndex = 2;
            waitForRendering(pane);
            verify(findChild(pane, "rawBodyArea").width > stack.width - 30);

            findChild(pane, "tabs").currentIndex = 3;
            waitForRendering(pane);
            compare(findChild(pane, "passwordField").width, stack.width);
        }

        /*! Verifies that the rename dialog renames through its accept path. */
        function test_renameDialogRenames() {
            const pane = createCollection();
            pane.selectedId = "r2";
            const dialog = findChild(pane, "renameDialog");

            pane.openRename();
            tryCompare(dialog, "opened", true);
            const field = findChild(pane, "renameField");
            compare(field.text, "List items");

            field.text = "From dialog";
            dialog.accept();
            compare(cppCollection.nodeName("r2"), "From dialog");
        }

        /*! Verifies that the rename dialog cannot be confirmed with a blank name. */
        function test_renameDialogRefusesBlankName() {
            const pane = createCollection();
            pane.selectedId = "r2";
            const dialog = findChild(pane, "renameDialog");

            pane.openRename();
            tryCompare(dialog, "opened", true);
            const ok = dialog.standardButton(Dialog.Ok);
            compare(ok.enabled, true);

            findChild(pane, "renameField").text = "   ";
            compare(ok.enabled, false);
            dialog.reject();
        }

        /*! Verifies that the delete dialog names the node as it is called now. */
        function test_deleteDialogShowsCurrentName() {
            const pane = createCollection();
            pane.selectedId = "r2";
            const dialog = findChild(pane, "deleteDialog");

            pane.openDelete();
            tryCompare(dialog, "opened", true);
            verify(findChild(pane, "deleteLabel").text.indexOf("List items") >= 0);
            dialog.reject();
            tryCompare(dialog, "visible", false);

            verify(cppCollection.rename("r2", "Renamed"));
            pane.openDelete();
            tryCompare(dialog, "opened", true);
            verify(findChild(pane, "deleteLabel").text.indexOf("Renamed") >= 0);

            dialog.accept();
            verify(!cppCollection.indexOfNode("r2").valid);
        }

        /*! Verifies typing in a table with the real model keeps the row and focus. */
        function test_realTableKeepsFocusWhileTyping() {
            const pane = createRequest("r1");
            findChild(pane, "tabs").currentIndex = 1;
            const table = findChild(pane, "headersTable");
            waitForRendering(pane);

            const rows = findChild(table, "rows");
            const row = rows.itemAtIndex(0);
            const field = findChild(row, "valueField");
            typeInto(field);
            keyClick(Qt.Key_Y);

            compare(cppRequestEditor.headersModel.rowCount(), 1);
            verify(field.activeFocus);
            compare(rows.itemAtIndex(0), row);
            compare(field.text, "*/*xy");
            verify(cppWorkspaceManager.dirty);
        }

        /*! Verifies that the tabs show their content. */
        function test_tabsSwitchContent() {
            const pane = createRequest("r1");
            const tabs = findChild(pane, "tabs");
            const stack = findChild(pane, "tabStack");

            for (let tab = 0; tab < tabs.count; ++tab) {
                tabs.currentIndex = tab;
                compare(stack.currentIndex, tab);
            }
            compare(tabs.count, 4);
        }

        /*! Verifies that the body controls follow the body mode. */
        function test_bodyModeSelectsEditor() {
            const pane = createRequest("r1");
            findChild(pane, "tabs").currentIndex = 2;

            compare(findChild(pane, "rawBodyArea").visible, true);
            compare(findChild(pane, "rawBodyArea").text, "{}");
            compare(findChild(pane, "formTable").visible, false);

            cppRequestEditor.bodyMode = 2;
            compare(findChild(pane, "rawBodyArea").visible, false);
            compare(findChild(pane, "formTable").visible, true);
        }

        /*! Verifies that typing in the raw body reaches the request. */
        function test_typingRawBodyWritesThrough() {
            const pane = createRequest("r1");
            findChild(pane, "tabs").currentIndex = 2;
            const area = findChild(pane, "rawBodyArea");

            area.forceActiveFocus();
            area.cursorPosition = area.length;
            keyClick(Qt.Key_X);

            compare(cppRequestEditor.rawBody, "{}x");
        }

        /*! Verifies that the password is hidden and writes through. */
        function test_authFieldsFollowType() {
            const pane = createRequest("r1");
            findChild(pane, "tabs").currentIndex = 3;

            const password = findChild(pane, "passwordField");
            compare(password.visible, true);
            compare(password.text, "pass");
            compare(password.echoMode, TextInput.Password);
            compare(findChild(pane, "tokenField").visible, false);

            cppRequestEditor.authType = 2; // AuthType::Bearer
            compare(password.visible, false);
            compare(findChild(pane, "tokenField").visible, true);
            compare(findChild(pane, "tokenField").echoMode, TextInput.Password);
        }
    }
}
