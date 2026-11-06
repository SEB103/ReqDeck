// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtTest
import Base
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
            property alias response: responsePane

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
                width: 400
                height: 600
            }

            ResponsePane {
                id: responsePane

                x: 700
                width: 300
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
            compare(findChild(pane, "sendButton").enabled, true);
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

        /*! Creates the wired panes with r2 open, pointing at \a url, ready to send. */
        function createSender(url) {
            const host = createWired();
            clickRow(host, 1);
            cppRequestEditor.url = url;
            waitForRendering(host);
            return host;
        }

        /*! Sends through the button and waits for the result. */
        function sendAndWait(host) {
            mouseClick(findChild(host.request, "sendButton"));
            tryCompare(cppResponse, "busy", false, 10000);
            waitForRendering(host);
        }

        /*! Verifies that a sent request shows status, details, and body. */
        function test_sendShowsStatusAndBody() {
            const host = createSender(cppTestHost.serverUrl("/status/201"));
            verify(findChild(host.response, "placeholder").visible);

            sendAndWait(host);

            compare(findChild(host.response, "placeholder").visible, false);
            compare(findChild(host.response, "statusChip").text, "201 Created");
            compare(findChild(host.response, "statusChip").statusClass, 2);
            compare(findChild(host.response, "sizeLabel").text, "10 B");
            compare(findChild(host.response, "requestNameLabel").text, "List items");
            compare(findChild(host.response, "bodyArea").text, "status 201");
        }

        /*! Verifies that a value being typed counts when Send is clicked. */
        function test_sendCommitsFocusedField() {
            const host = createSender(cppTestHost.serverUrl("/status/200"));
            findChild(host.request, "tabs").currentIndex = 3;
            waitForRendering(host);

            const box = findChild(host.request, "timeoutBox");
            box.contentItem.forceActiveFocus();
            box.contentItem.selectAll();
            keyClick(Qt.Key_9);
            compare(cppRequestEditor.timeoutMs, 0);

            sendAndWait(host);

            compare(cppRequestEditor.timeoutMs, 9);
        }

        /*! Verifies cancelling from the response pane. */
        function test_cancelDuringDelay() {
            const host = createSender(cppTestHost.serverUrl("/delay/3000"));
            mouseClick(findChild(host.request, "sendButton"));
            verify(cppResponse.busy);
            waitForRendering(host);
            compare(findChild(host.response, "busyColumn").visible, true);
            compare(findChild(host.request, "sendButton").text, "Cancel");

            mouseClick(findChild(host.response, "cancelButton"));

            compare(cppResponse.busy, false);
            tryCompare(findChild(host.response, "errorLabel"), "visible", true);
            compare(findChild(host.request, "sendButton").text, "Send");
        }

        /*! Verifies that the send button cancels while a request runs. */
        function test_sendButtonCancels() {
            const host = createSender(cppTestHost.serverUrl("/delay/3000"));
            const button = findChild(host.request, "sendButton");
            mouseClick(button);
            verify(cppResponse.busy);

            mouseClick(button);

            compare(cppResponse.busy, false);
        }

        /*! Verifies that a transport failure appears in the pane. */
        function test_errorShowsInPane() {
            const host = createSender(cppTestHost.closedUrl());

            sendAndWait(host);

            compare(findChild(host.response, "statusChip").visible, false);
            compare(findChild(host.response, "errorLabel").visible, true);
            verify(findChild(host.response, "errorLabel").text.length > 0);
        }

        /*! Verifies that a request that cannot be built appears in the pane. */
        function test_buildErrorShowsInPane() {
            const host = createSender("{{nope}}/x");

            sendAndWait(host);

            verify(findChild(host.response, "errorLabel").text.indexOf("{{nope}}") >= 0);
        }

        /*! Verifies the switch between formatted and original text. */
        function test_prettyRawSwitch() {
            const host = createSender(cppTestHost.serverUrl("/echo"));
            sendAndWait(host);

            const area = findChild(host.response, "bodyArea");
            const toggle = findChild(host.response, "prettySwitch");
            compare(toggle.visible, true);
            compare(toggle.checked, true);
            verify(area.text.indexOf("\n") >= 0);

            mouseClick(toggle);

            compare(cppResponse.pretty, false);
            verify(area.text.indexOf("\n") < 0);
            cppResponse.pretty = true;
        }

        /*! Verifies that formatted JSON is colored. */
        function test_bodyIsColored() {
            const host = createSender(cppTestHost.serverUrl("/echo"));
            sendAndWait(host);
            const area = findChild(host.response, "bodyArea");
            // Line 2 holds the first key; the first line is only a brace.
            verify(cppTestHost.isColoredBelowFirstLine(area.textDocument));
        }

        /*! Verifies the header list. */
        function test_headersTabListsHeaders() {
            const host = createSender(cppTestHost.serverUrl("/response-headers"));
            sendAndWait(host);

            findChild(host.response, "responseTabs").currentIndex = 1;
            waitForRendering(host);

            const list = findChild(host.response, "headersList");
            verify(list.count >= 3);
        }

        /*! Verifies that a header value cannot make the application load anything. */
        function test_headerMarkupStaysText() {
            const host = createSender(cppTestHost.serverUrl("/markup"));
            sendAndWait(host);
            const requests = cppTestHost.requestCount();

            findChild(host.response, "responseTabs").currentIndex = 1;
            waitForRendering(host);
            wait(300);

            compare(cppTestHost.requestCount(), requests);
            const list = findChild(host.response, "headersList");
            let shown = "";
            for (let row = 0; row < list.count; ++row) {
                const value = findChild(list.itemAtIndex(row), "headerValue");
                if (value.text.indexOf("<img") >= 0)
                    shown = value.text;
                compare(value.textFormat, Text.PlainText);
                compare(findChild(list.itemAtIndex(row), "headerName").textFormat, Text.PlainText);
            }
            verify(shown.length > 0);
        }

        /*! Verifies that other text from outside is plain too. */
        function test_serverAndFileTextIsPlain() {
            const host = createSender(cppTestHost.serverUrl("/status/200"));
            cppRequestEditor.name = "<b>name</b>";
            sendAndWait(host);

            compare(findChild(findChild(host.response, "statusChip"), "label").textFormat, Text.PlainText);
            compare(findChild(host.response, "requestNameLabel").textFormat, Text.PlainText);
            compare(findChild(host.response, "errorLabel").textFormat, Text.PlainText);
            compare(findChild(host.response, "warningLabel").textFormat, Text.PlainText);
            const row = findChild(host.collection, "tree").itemAtCell(0, 1);
            compare(findChild(row, "nodeName").textFormat, Text.PlainText);
        }

        /*! Verifies that a long unbroken body keeps the window responsive. */
        function test_longUnbrokenBodyStaysResponsive() {
            const host = createSender(cppTestHost.serverUrl("/long/262144"));
            const started = Date.now();

            sendAndWait(host);

            const elapsed = Date.now() - started;
            compare(findChild(host.response, "bodyArea").wrapMode, TextEdit.WrapAnywhere);
            verify(elapsed < 10000, "took " + elapsed + " ms");
        }

        /*! Verifies that a theme switch recolors a large body without a hang. */
        function test_themeSwitchStaysResponsive() {
            const host = createSender(cppTestHost.serverUrl("/json/3000"));
            sendAndWait(host);
            const area = findChild(host.response, "bodyArea");
            verify(area.length > 50000);
            verify(cppTestHost.isColoredBelowFirstLine(area.textDocument));
            const shownText = area.text;

            const started = Date.now();
            BsTheme.dark = !BsTheme.dark;
            waitForRendering(host);
            const elapsed = Date.now() - started;
            BsTheme.dark = false;

            verify(elapsed < 5000, "took " + elapsed + " ms");
            compare(area.text, shownText);
            verify(cppTestHost.isColoredBelowFirstLine(area.textDocument));
        }

        /*! Verifies that the header row stays inside a narrow pane. */
        function test_narrowPaneKeepsDetailsInside() {
            const host = createSender(cppTestHost.serverUrl("/status/503"));
            sendAndWait(host);
            host.response.width = 244;
            waitForRendering(host);

            for (const name of ["statusChip", "timeLabel", "sizeLabel", "responseTabs"]) {
                const item = findChild(host.response, name);
                const right = item.mapToItem(host.response, item.width, 0).x;
                verify(right <= 244, name + " ends at " + right);
            }
        }

        /*! Verifies that a body that cannot be formatted wraps and offers no switch. */
        function test_brokenJsonStaysReadable() {
            const host = createSender(cppTestHost.serverUrl("/truncated"));
            sendAndWait(host);

            compare(findChild(host.response, "prettySwitch").visible, false);
            compare(findChild(host.response, "bodyArea").wrapMode, TextEdit.Wrap);
        }

        /*! Verifies that binary data is described, not shown. */
        function test_binaryNoticeShown() {
            const host = createSender(cppTestHost.serverUrl("/binary"));
            sendAndWait(host);

            compare(findChild(host.response, "bodyArea").text, "Binary body, 8 B. Not shown.");
            compare(findChild(host.response, "prettySwitch").visible, false);
        }

        /*! Verifies that another workspace removes the response. */
        function test_workspaceSwitchClearsResponse() {
            const host = createSender(cppTestHost.serverUrl("/status/200"));
            sendAndWait(host);

            verify(cppTestHost.reset());

            tryCompare(findChild(host.response, "placeholder"), "visible", true);
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
