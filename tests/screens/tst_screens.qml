// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
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

            combo.currentIndex = combo.find("DELETE");
            combo.activated(combo.currentIndex);

            compare(cppRequestEditor.method, "DELETE");
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
