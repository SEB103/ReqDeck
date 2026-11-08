// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
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

    Component {
        id: dialogComponent

        Item {
            property alias dialog: environmentDialog

            anchors.fill: parent

            EnvironmentDialog {
                id: environmentDialog

                width: 760
                height: 500
            }
        }
    }

    Component {
        id: historyComponent

        HistoryPane {
            width: 300
            height: 500
            onEntryOpened: (id) => cppRequestEditor.load(id)
        }
    }

    Component {
        id: curlComponent

        Item {
            property alias dialog: curlDialog

            anchors.fill: parent

            CurlImportDialog {
                id: curlDialog

                width: 640
                height: 420
                onImported: (id) => cppRequestEditor.load(id)
            }
        }
    }

    Component {
        id: mainComponent

        Main {}
    }

    Component {
        id: helpTargetsComponent

        Item {
            property alias field: helpField
            property alias inner: innerItem
            property alias plain: plainItem
            property alias disabledButton: offButton

            property string helpTopic: "outer"

            width: 400
            height: 300

            Rectangle {
                id: innerItem

                property string helpTopic: "inner"

                width: 100
                height: 100

                Rectangle {
                    id: plainItem

                    x: 10
                    y: 10
                    width: 30
                    height: 30
                }
            }

            TextField {
                id: helpField

                property string helpTopic: "field"

                x: 200
                width: 150
            }

            Button {
                id: offButton

                property string helpTopic: "disabled"

                y: 200
                enabled: false
                text: "Off"
            }
        }
    }

    // Connected like Main.qml connects the top bar.
    Component {
        id: selectorComponent

        BsEnvironmentSelector {
            names: cppEnvironments.names
            activeIndex: cppEnvironments.activeIndex
            onEnvironmentSelected: (index) => cppEnvironments.activeIndex = index
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
            // A closed application window leaves this one inactive at times,
            // and shortcuts only reach an active window.
            root.Window.window.requestActivate();
            tryVerify(() => root.Window.window.active);
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

        /*! Verifies that the tab bars of the panes are compact. */
        function test_tabBarsAreCompact() {
            const host = createWired();
            clickRow(host, 1);
            cppRequestEditor.url = cppTestHost.serverUrl("/status/200");
            sendAndWait(host);

            compare(findChild(host.request, "tabs").height, BsTheme.compactBarHeight);
            compare(findChild(host.response, "responseTabs").height, BsTheme.compactBarHeight);
        }

        /*! Verifies that the tests see the style settings of the application. */
        function test_stylesMatchApplication() {
            const field = findChild(createRequest("r2"), "nameField");
            verify(field.height <= 48, "height " + field.height);
        }

        /*! Verifies that the name stores on leaving and gives way when blank. */
        function test_inlineNameStoresAndRestores() {
            const pane = createRequest("r2");
            const name = findChild(pane, "nameField");
            name.forceActiveFocus();
            name.selectAll();
            keyClick(Qt.Key_Delete);
            keyClick(Qt.Key_Return);
            compare(name.text, "List items");

            name.selectAll();
            keyClick(Qt.Key_X);
            findChild(pane, "methodCombo").forceActiveFocus();
            compare(cppCollection.nodeName("r2"), "x");
            compare(cppRequestEditor.name, "x");

            cppRequestEditor.load("r1");
            compare(name.text, "Create item");
        }

        /*! Verifies that the name is a title, not a boxed field. */
        function test_nameIsCompactTitle() {
            const pane = createRequest("r2");
            const name = findChild(pane, "nameField");

            compare(name.height, 30);
            compare(name.placeholderText, "");
            compare(name.Accessible.name, "Request name");
        }

        /*! Verifies that a long name leaves the rows below where they are. */
        function test_longNameKeepsLayout() {
            const pane = createRequest("r2");
            const url = findChild(pane, "urlField");
            const y = url.y;

            cppRequestEditor.name = "n".repeat(300);
            waitForRendering(pane);

            compare(findChild(pane, "nameField").height, 30);
            compare(url.y, y);
        }

        /*! Verifies that a long name shows its start, as the tree does. */
        function test_longNameShowsItsStart() {
            const pane = createRequest("r2");
            const name = findChild(pane, "nameField");

            cppRequestEditor.name = "START" + "n".repeat(300);
            waitForRendering(pane);
            verify(name.positionToRectangle(0).x >= 0, "x " + name.positionToRectangle(0).x);

            name.forceActiveFocus();
            name.cursorPosition = name.length;
            keyClick(Qt.Key_X);
            findChild(pane, "methodCombo").forceActiveFocus();
            waitForRendering(pane);

            verify(name.positionToRectangle(0).x >= 0, "x " + name.positionToRectangle(0).x);
        }

        /*! Verifies that the tabs fit the narrowest request pane. */
        function test_tabsFitMinimumPane() {
            const pane = createRequest("r2");
            pane.width = 300;
            waitForRendering(pane);
            const tabs = findChild(pane, "tabs");

            for (let i = 0; i < tabs.count; ++i) {
                const tab = tabs.itemAt(i);
                verify(tab.implicitContentWidth <= tab.availableWidth,
                       tab.text + ": " + tab.implicitContentWidth + " in " + tab.availableWidth);
            }
        }

        /*! Verifies that the URL reads like code. */
        function test_urlUsesMonospace() {
            compare(findChild(createRequest("r2"), "urlField").font.family, "Consolas");
        }

        /*! Verifies that Send lines up with the URL. */
        function test_sendButtonIsLevelWithUrl() {
            const pane = createRequest("r2");
            const send = findChild(pane, "sendButton");
            const url = findChild(pane, "urlField");

            compare(send.height, url.height);
            compare(send.y, url.y);
            compare(send.topInset, 0);
        }

        /*! Verifies that the method box shows the color of its method. */
        function test_methodBoxShowsMethodColor() {
            const pane = createRequest("r1");
            const combo = findChild(pane, "methodCombo");

            verify(Qt.colorEqual(combo.contentItem.color, BsTheme.methodColor("POST")));
        }

        /*! Verifies that each offered method has its own color. */
        function test_methodOptionsUseTheirColors() {
            const combo = findChild(createRequest("r2"), "methodCombo");
            combo.popup.open();
            tryCompare(combo.popup, "opened", true);

            const option = combo.popup.contentItem.itemAtIndex(4);

            verify(Qt.colorEqual(option.contentItem.color, BsTheme.methodColor("DELETE")));
            compare(option.text, "DELETE");
            combo.popup.close();
            tryCompare(combo.popup, "visible", false);
        }

        /*! Verifies that the request row still fits a narrow pane. */
        function test_requestRowFitsNarrowPane() {
            const pane = createRequest("r2");
            pane.width = 300; // SplitView.minimumWidth of the request pane
            waitForRendering(pane);
            const combo = findChild(pane, "methodCombo");
            const url = findChild(pane, "urlField");
            const send = findChild(pane, "sendButton");
            const tabs = findChild(pane, "tabs");

            verify(url.width >= 70, "url " + url.width);
            verify(send.mapToItem(pane, send.width, 0).x <= pane.width,
                   "send ends at " + send.mapToItem(pane, send.width, 0).x);
            verify(combo.mapToItem(pane, 0, 0).x >= 0);
            verify(tabs.mapToItem(pane, tabs.width, 0).x <= pane.width);
        }

        /*! Verifies that a method is shown in its own color. */
        function test_treeShowsMethodColor() {
            const tree = findChild(createCollection(), "tree");

            const label = findChild(tree.itemAtCell(0, 1), "nodeMethod");

            verify(Qt.colorEqual(label.color, BsTheme.methodColor("GET")));
        }

        /*! Verifies that the selected row is a tint, on which the method stays visible. */
        function test_selectedRowKeepsMethodVisible() {
            const pane = createCollection();
            const tree = findChild(pane, "tree");

            pane.selectedId = "r2";

            const row = tree.itemAtCell(0, 1);
            verify(Qt.colorEqual(row.background.color, BsTheme.selectedRowColor));
            verify(Qt.colorEqual(findChild(row, "nodeMethod").color, BsTheme.methodColor("GET")));
            compare(findChild(row, "selectionBar").visible, true);
        }

        /*! Verifies that a row without selection has no band of its own. */
        function test_plainRowHasNoBackground() {
            const tree = findChild(createCollection(), "tree");

            compare(tree.itemAtCell(0, 1).background.color.a, 0);
        }

        /*! Verifies that the tree stays compact. */
        function test_treeRowsAreCompact() {
            const tree = findChild(createCollection(), "tree");

            compare(tree.itemAtCell(0, 1).height, 30);
        }

        /*! Verifies that the result of an entry shows the class of its status. */
        function test_historyShowsStatusColor() {
            cppTestHost.addHistory("http://x", 1, "", 404);

            const row = findChild(createHistory(), "historyList").itemAtIndex(0);

            verify(Qt.colorEqual(findChild(row, "historyResult").color, BsTheme.statusColor(404)));
            verify(Qt.colorEqual(findChild(row, "historyMethod").color, BsTheme.methodColor("GET")));
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

        /*! Returns the text area inside the URL field of \a pane. */
        function urlArea(pane) {
            return findChild(findChild(pane, "urlField"), "editArea");
        }

        /*! Verifies that Return and Enter add nothing to the URL. */
        function test_returnAddsNothing() {
            const field = findChild(createRequest("r1"), "urlField");
            field.forceActiveFocus();

            keyClick(Qt.Key_Return);
            keyClick(Qt.Key_Enter);

            compare(cppRequestEditor.url, "http://h/items");
            verify(!cppWorkspaceManager.dirty);
        }

        /*! Verifies that Tab leaves the URL field without writing a tab. */
        function test_tabLeavesTheField() {
            const field = findChild(createRequest("r1"), "urlField");
            field.forceActiveFocus();
            verify(field.activeFocus);

            keyClick(Qt.Key_Tab);

            verify(!field.activeFocus);
            compare(cppRequestEditor.url, "http://h/items");
        }

        /*! Verifies that line breaks in inserted text do not reach the URL. */
        function test_insertedLineBreaksAreRemoved() {
            const pane = createRequest("r2");
            const area = urlArea(pane);
            area.forceActiveFocus();

            area.insert(0, "http://a\r\n/b\n");

            tryCompare(cppRequestEditor, "url", "http://a/b");
            compare(area.text, "http://a/b");
            compare(area.cursorPosition, 10);
        }

        /*! Verifies that showing a URL with a line break changes nothing. */
        function test_showingRequestLeavesItClean() {
            const pane = createRequest("r2");
            cppRequestEditor.url = "http://a\r\nb";
            verify(cppWorkspaceManager.saveWorkspace());

            cppRequestEditor.load("r1");
            waitForRendering(pane);
            cppRequestEditor.load("r2");
            waitForRendering(pane);

            compare(cppRequestEditor.url, "http://a\r\nb");
            verify(!cppWorkspaceManager.dirty);
        }

        function test_recoloringLeavesUrlAlone_data() {
            return [
                {tag: "line break", url: "http://a\r\nb"},
                {tag: "no-break space", url: "http://a\u00A0b"}
            ];
        }

        /*! Verifies that recoloring a shown URL is not taken for an edit. */
        function test_recoloringLeavesUrlAlone(data) {
            const pane = createRequest("r2");
            cppRequestEditor.url = data.url;
            waitForRendering(pane);
            verify(cppWorkspaceManager.saveWorkspace());
            wait(50);
            compare(cppRequestEditor.url, data.url);
            verify(!cppWorkspaceManager.dirty);

            cppEnvironments.activeIndex = 0;
            wait(50);
            compare(cppRequestEditor.url, data.url);

            verify(cppWorkspaceManager.saveWorkspace());
            const dark = cppEnvironments.darkTheme;
            cppEnvironments.darkTheme = !dark;
            wait(50);
            cppEnvironments.darkTheme = dark;
            wait(50);
            compare(cppRequestEditor.url, data.url);
            verify(!cppWorkspaceManager.dirty);
        }

        /*! Verifies that a request change drops line breaks still to be removed. */
        function test_shownUrlKeepsLineBreakAfterPendingPaste() {
            const pane = createRequest("r2");
            cppRequestEditor.url = "";
            verify(cppWorkspaceManager.saveWorkspace());
            cppRequestEditor.load("r1");
            cppRequestEditor.url = "http://stored\r\nbreak";
            cppRequestEditor.load("r2");
            waitForRendering(pane);

            urlArea(pane).insert(0, "a\nb");
            cppRequestEditor.load("r1");
            waitForRendering(pane);

            compare(cppRequestEditor.url, "http://stored\r\nbreak");
        }

        /*! Verifies that a long URL shows and edits in reasonable time. */
        function test_longUrlStaysResponsive() {
            const pane = createRequest("r2");
            const field = findChild(pane, "urlField");
            const started = Date.now();

            cppRequestEditor.url = "http://h/" + "a".repeat(262144);
            waitForRendering(pane);
            typeInto(field);

            const elapsed = Date.now() - started;
            compare(field.length, 262154);
            verify(elapsed < 10000, "took " + elapsed + " ms");
        }

        /*! Verifies that the view follows the cursor through a long URL. */
        function test_urlKeepsCursorVisible() {
            const pane = createRequest("r2");
            const field = findChild(pane, "urlField");
            const area = urlArea(pane);
            cppRequestEditor.url = "http://h/" + "a".repeat(400);
            waitForRendering(pane);

            field.forceActiveFocus();
            field.cursorPosition = field.length;
            waitForRendering(pane);

            let x = area.mapToItem(field, area.cursorRectangle.x, 0).x;
            verify(x >= 0 && x <= field.width, "cursor at " + x);

            field.cursorPosition = 0;
            waitForRendering(pane);

            x = area.mapToItem(field, area.cursorRectangle.x, 0).x;
            verify(x >= 0 && x <= field.width, "cursor at " + x);
        }

        /*! Verifies that a reference is marked until it resolves. */
        function test_unresolvedReferenceIsHighlighted() {
            const pane = createRequest("r2");
            const field = findChild(pane, "urlField");
            cppRequestEditor.url = "{{baseUrl}}/x";

            tryVerify(() => cppTestHost.hasUnderline(field.textDocument));

            cppEnvironments.activeIndex = 0;

            verify(!cppTestHost.hasUnderline(field.textDocument));
        }

        /*! Verifies that the URL field lines up with the method box. */
        function test_urlRowIsLevel() {
            const pane = createRequest("r2");
            const field = findChild(pane, "urlField");
            const combo = findChild(pane, "methodCombo");

            compare(field.height, combo.height);
            compare(field.y, combo.y);
        }

        /*! Verifies that the URL field never reads its text as markup. */
        function test_urlIsPlainText() {
            const pane = createRequest("r2");

            cppRequestEditor.url = "<b>x</b>";
            waitForRendering(pane);

            compare(urlArea(pane).textFormat, TextEdit.PlainText);
            compare(findChild(pane, "urlField").length, 8);
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

        /*! Creates the history pane and waits until its list is laid out. */
        function createHistory() {
            const pane = createTemporaryObject(historyComponent, root);
            verify(pane !== null);
            waitForRendering(pane);
            return pane;
        }

        /*! Double clicks \a row of the history in \a pane. */
        function openHistoryRow(pane, row) {
            const item = findChild(pane, "historyList").itemAtIndex(row);
            mouseDoubleClickSequence(item);
        }

        /*! Verifies that a sent request shows up with its result. */
        function test_historyShowsSentRequest() {
            const url = cppTestHost.serverUrl("/status/201");
            const host = createSender(url);
            const pane = createHistory();

            sendAndWait(host);
            waitForRendering(pane);

            const list = findChild(pane, "historyList");
            compare(list.count, 1);
            const row = list.itemAtIndex(0);
            compare(findChild(row, "historyMethod").text, "GET");
            compare(findChild(row, "historyUrl").text, url);
            verify(findChild(row, "historyResult").text.indexOf("201") === 0);
        }

        /*! Verifies that a failure shows its reason instead of a status. */
        function test_historyShowsFailure() {
            cppTestHost.addHistory("http://x", 1, "Connection refused");

            const row = findChild(createHistory(), "historyList").itemAtIndex(0);

            compare(findChild(row, "historyResult").text, "Connection refused");
        }

        /*! Verifies that a failure after the status line does not read as success. */
        function test_historyShowsFailureAfterStatus() {
            cppTestHost.addHistory("http://x", 1, "Connection closed", 200);

            const row = findChild(createHistory(), "historyList").itemAtIndex(0);

            const result = findChild(row, "historyResult").text;
            verify(result.indexOf("200") === 0, result);
            verify(result.indexOf("Connection closed") > 0, result);
        }

        /*! Verifies that the send shortcut does not open the chosen entry. */
        function test_historyControlReturnOpensNothing() {
            cppTestHost.addHistory("http://x", 1);
            const list = findChild(createHistory(), "historyList");
            list.forceActiveFocus();
            keyClick(Qt.Key_Down);
            compare(list.currentIndex, 0);

            keyClick(Qt.Key_Return, Qt.ControlModifier);
            keyClick(Qt.Key_Enter, Qt.ControlModifier);

            compare(cppWorkspaceManager.dirty, false);
            compare(cppRequestEditor.hasRequest, false);
        }

        /*! Verifies that the keypad Enter key opens the chosen entry. */
        function test_historyKeypadEnterOpensEntry() {
            cppTestHost.addHistory("http://x", 1);
            const list = findChild(createHistory(), "historyList");
            list.forceActiveFocus();
            keyClick(Qt.Key_Down);

            keyClick(Qt.Key_Enter, Qt.KeypadModifier);

            compare(cppRequestEditor.url, "http://x");
        }

        /*! Verifies that opening an entry adds a request and shows it. */
        function test_openingEntryCreatesRequest() {
            cppTestHost.addHistory("http://x/items", 1);
            const pane = createHistory();
            verify(cppWorkspaceManager.saveWorkspace());

            openHistoryRow(pane, 0);

            compare(cppRequestEditor.url, "http://x/items");
            compare(cppRequestEditor.name, "GET http://x/items");
            verify(cppWorkspaceManager.dirty);
        }

        /*! Verifies that a name being edited is stored before an entry opens. */
        function test_openingEntryStoresNameBeingEdited() {
            cppTestHost.addHistory("http://x/items", 1);
            const request = createRequest("r2");
            const pane = createHistory();
            const name = findChild(request, "nameField");
            name.forceActiveFocus();
            name.selectAll();
            keyClick(Qt.Key_X);

            openHistoryRow(pane, 0);

            compare(cppCollection.nodeName("r2"), "x");
        }

        /*! Verifies what an opened entry carries over. */
        function test_openedEntryKeepsReferences() {
            const host = createWired();
            clickRow(host, 1);
            cppEnvironments.activeIndex = 0;
            cppRequestEditor.url = "{{baseUrl}}";
            cppRequestEditor.authType = 1; // AuthType::Basic
            cppRequestEditor.authUsername = "{{user}}";
            cppRequestEditor.authPassword = "literal";
            sendAndWait(host);

            openHistoryRow(createHistory(), 0);

            compare(cppRequestEditor.url, "{{baseUrl}}");
            compare(cppRequestEditor.authUsername, "{{user}}");
            compare(cppRequestEditor.authPassword, "");
        }

        /*! Verifies keyboard use of the list. */
        function test_historyWorksByKeyboard() {
            cppTestHost.addHistory("http://older", 1);
            cppTestHost.addHistory("http://newer", 1);
            const list = findChild(createHistory(), "historyList");
            verify(list.activeFocusOnTab);
            list.forceActiveFocus();

            keyClick(Qt.Key_Down);
            keyClick(Qt.Key_Down);
            keyClick(Qt.Key_Return);

            compare(cppRequestEditor.url, "http://older");
        }

        /*! Verifies that Return does nothing before a row is chosen. */
        function test_historyReturnWithoutRowDoesNothing() {
            cppTestHost.addHistory("http://x", 1);
            const list = findChild(createHistory(), "historyList");
            list.forceActiveFocus();

            keyClick(Qt.Key_Return);

            compare(cppRequestEditor.hasRequest, false);
            compare(cppWorkspaceManager.dirty, false);
        }

        /*! Verifies that clearing waits for a confirmation. */
        function test_clearAsksFirst() {
            cppTestHost.addHistory("http://x", 2);
            const pane = createHistory();
            const list = findChild(pane, "historyList");
            const confirmation = findChild(pane, "clearConfirmation");

            mouseClick(findChild(pane, "clearHistory"));

            tryCompare(confirmation, "opened", true);
            compare(list.count, 2);

            confirmation.accept();

            compare(list.count, 0);
            compare(findChild(pane, "clearHistory").enabled, false);
        }

        /*! Verifies that text from the history file cannot fetch anything. */
        function test_historyMarkupStaysText() {
            const requests = cppTestHost.requestCount();
            const markup = '<img src="' + cppTestHost.serverUrl("/x") + '">';
            cppTestHost.addHistory(markup, 1);
            cppTestHost.addHistory(markup, 1, markup);

            const pane = createHistory();
            wait(200);

            compare(cppTestHost.requestCount(), requests);
            compare(findChild(findChild(pane, "historyList").itemAtIndex(0), "historyUrl").textFormat,
                    Text.PlainText);
        }

        /*! Verifies the hint of an empty history. */
        function test_emptyHistoryShowsHint() {
            const pane = createHistory();

            verify(findChild(pane, "emptyHint").visible);

            cppTestHost.addHistory("http://x", 1);
            tryCompare(findChild(pane, "emptyHint"), "visible", false);
        }

        /*! Verifies that a full history shows in reasonable time. */
        function test_fullHistoryStaysResponsive() {
            cppTestHost.addHistory("http://h/" + "a".repeat(2000), 200);
            const started = Date.now();

            const list = findChild(createHistory(), "historyList");

            const elapsed = Date.now() - started;
            compare(list.count, 200);
            verify(elapsed < 2000, "took " + elapsed + " ms");
        }

        /*! Opens the import dialog and waits until it is shown. */
        function openCurl() {
            const host = createTemporaryObject(curlComponent, root);
            verify(host !== null);
            host.dialog.open();
            tryCompare(host.dialog, "opened", true);
            return host;
        }

        /*! Puts \a command into the dialog of \a host and waits for the check. */
        function enterCommand(host, command) {
            findChild(host, "curlInput").text = command;
            wait(0);
            waitForRendering(findChild(host, "curlInput"));
        }

        /*! Verifies that Import needs a valid command, and imports it. */
        function test_curlDialogEnablesImportForValidCommand() {
            const host = openCurl();
            const button = findChild(host, "importButton");
            compare(button.enabled, false);

            enterCommand(host, "curl -X DELETE http://x/items/1");
            compare(button.enabled, true);
            verify(findChild(host, "curlStatus").text.indexOf("DELETE http://x/items/1") >= 0);

            mouseClick(button);

            tryCompare(host.dialog, "visible", false);
            compare(cppRequestEditor.method, "DELETE");
            compare(cppRequestEditor.url, "http://x/items/1");
        }

        /*! Verifies that a broken command says why. */
        function test_curlDialogShowsError() {
            const host = openCurl();

            enterCommand(host, "not a command");

            compare(findChild(host, "importButton").enabled, false);
            verify(findChild(host, "curlStatus").text.length > 0);
            verify(findChild(host, "curlStatus").text.indexOf("Paste") < 0);
        }

        /*! Verifies that options without an equivalent are named. */
        function test_curlDialogShowsIgnoredOptions() {
            const host = openCurl();

            enterCommand(host, "curl --compressed http://x");

            verify(findChild(host, "curlStatus").text.indexOf("--compressed") >= 0);
            compare(findChild(host, "importButton").enabled, true);
        }

        /*! Verifies that a pasted command cannot fetch anything. */
        function test_curlDialogShowsMarkupAsText() {
            const requests = cppTestHost.requestCount();
            const host = openCurl();

            enterCommand(host, "curl 'http://x/<img src=\"" + cppTestHost.serverUrl("/x") + "\">'");
            wait(200);

            compare(cppTestHost.requestCount(), requests);
            compare(findChild(host, "curlStatus").textFormat, Text.PlainText);
            compare(findChild(host, "curlInput").textFormat, TextEdit.PlainText);
        }

        /*! Verifies that a huge paste neither hangs nor crashes the dialog. */
        function test_curlDialogSurvivesLongPaste() {
            const host = openCurl();
            const started = Date.now();

            enterCommand(host, "curl http://x -d '" + "a".repeat(262144) + "'");

            const elapsed = Date.now() - started;
            compare(findChild(host, "importButton").enabled, true);
            verify(elapsed < 5000, "took " + elapsed + " ms");
        }

        /*! Verifies that a name being edited is stored when the dialog takes the focus. */
        function test_curlImportStoresNameBeingEdited() {
            const pane = createRequest("r2");
            const name = findChild(pane, "nameField");
            name.forceActiveFocus();
            name.selectAll();
            keyClick(Qt.Key_X);

            openCurl();

            compare(cppCollection.nodeName("r2"), "x");
        }

        /*! Verifies that the dialog forgets the last command. */
        function test_curlDialogStartsEmpty() {
            const host = openCurl();
            enterCommand(host, "curl http://x");
            host.dialog.reject();
            tryCompare(host.dialog, "visible", false);

            host.dialog.open();
            tryCompare(host.dialog, "opened", true);
            waitForRendering(findChild(host, "curlInput"));

            compare(findChild(host, "curlInput").text, "");
            compare(findChild(host, "importButton").enabled, false);
        }

        /*! Presses the send shortcut and waits for the result. */
        function pressSend() {
            keyClick(Qt.Key_Return, Qt.ControlModifier);
            tryCompare(cppResponse, "busy", false, 10000);
        }

        /*! Verifies that the shortcut sends and leaves the focus where it was. */
        function test_shortcutSendsFromUrlField() {
            const host = createSender(cppTestHost.serverUrl("/status/201"));
            const field = findChild(host.request, "urlField");
            field.forceActiveFocus();

            pressSend();

            compare(cppResponse.statusCode, 201);
            verify(field.activeFocus);
            compare(cppRequestEditor.url, cppTestHost.serverUrl("/status/201"));
        }

        /*! Verifies that the keypad Enter key sends too. */
        function test_shortcutSendsWithEnterKey() {
            const host = createSender(cppTestHost.serverUrl("/status/202"));
            findChild(host.request, "urlField").forceActiveFocus();

            keyClick(Qt.Key_Enter, Qt.ControlModifier);
            tryCompare(cppResponse, "busy", false, 10000);

            compare(cppResponse.statusCode, 202);
        }

        /*! Verifies that the shortcut works from the raw body, which takes Return. */
        function test_shortcutSendsFromRawBody() {
            const host = createSender(cppTestHost.serverUrl("/status/201"));
            cppRequestEditor.bodyMode = 1; // BodyMode::Raw
            findChild(host.request, "tabs").currentIndex = 2;
            waitForRendering(host);
            const area = findChild(host.request, "rawBodyArea");
            area.forceActiveFocus();
            verify(area.activeFocus);

            pressSend();

            compare(cppResponse.statusCode, 201);
            compare(area.text, "");
        }

        /*! Verifies that a name being edited is stored before sending. */
        function test_shortcutStoresNameBeingEdited() {
            const host = createSender(cppTestHost.serverUrl("/status/201"));
            const name = findChild(host.request, "nameField");
            name.forceActiveFocus();
            name.selectAll();
            keyClick(Qt.Key_X);

            pressSend();

            compare(cppCollection.nodeName("r2"), "x");
            verify(name.activeFocus);
        }

        /*! Verifies that a timeout being edited is stored before sending. */
        function test_shortcutStoresTimeoutBeingEdited() {
            const host = createSender(cppTestHost.serverUrl("/status/201"));
            findChild(host.request, "tabs").currentIndex = 2;
            waitForRendering(host);
            const box = findChild(host.request, "timeoutBox");
            box.forceActiveFocus();
            keyClick(Qt.Key_A, Qt.ControlModifier);
            keyClick(Qt.Key_7);

            pressSend();

            compare(cppRequestEditor.timeoutMs, 7);
        }

        /*! Verifies that a running request is not sent twice. */
        function test_shortcutIgnoredWhileBusy() {
            const host = createSender(cppTestHost.serverUrl("/delay/300"));
            findChild(host.request, "urlField").forceActiveFocus();
            const requests = cppTestHost.requestCount();

            keyClick(Qt.Key_Return, Qt.ControlModifier);
            verify(cppResponse.busy);
            keyClick(Qt.Key_Return, Qt.ControlModifier);
            tryCompare(cppResponse, "busy", false, 10000);

            compare(cppTestHost.requestCount(), requests + 1);
        }

        /*! Verifies that nothing happens without an open request. */
        function test_shortcutIgnoredWithoutRequest() {
            const pane = createTemporaryObject(requestComponent, root);
            const requests = cppTestHost.requestCount();

            keyClick(Qt.Key_Return, Qt.ControlModifier);
            wait(50);

            verify(pane !== null);
            compare(cppResponse.busy, false);
            compare(cppResponse.hasResponse, false);
            compare(cppTestHost.requestCount(), requests);
        }

        /*! Verifies that a modal dialog blocks the shortcut. */
        function test_shortcutIgnoredUnderModalDialog() {
            createSender(cppTestHost.serverUrl("/status/201"));
            const requests = cppTestHost.requestCount();
            const host = openDialog();

            keyClick(Qt.Key_Return, Qt.ControlModifier);
            wait(50);

            compare(cppResponse.busy, false);
            compare(cppTestHost.requestCount(), requests);
            host.dialog.close();
            tryCompare(host.dialog, "visible", false);
        }

        /*! Creates the application window with r2 open at \a url and a name being typed. */
        function createMainTypingName(url) {
            const window = createTemporaryObject(mainComponent, root);
            verify(window !== null);
            window.requestActivate();
            tryCompare(window, "active", true);
            waitForRendering(window.contentItem);
            findChild(window.contentItem, "tree").parent.selectNode("r2");
            if (url !== undefined)
                cppRequestEditor.url = url;
            const name = findChild(window.contentItem, "nameField");
            name.forceActiveFocus();
            name.selectAll();
            cppTestHost.pressKey(window, Qt.Key_X, Qt.NoModifier);
            compare(name.text, "x");
            compare(cppCollection.nodeName("r2"), "List items");
            return window;
        }

        /*! Verifies that Ctrl+S saves a name still being typed. */
        function test_mainSaveShortcutStoresNameBeingEdited() {
            const window = createMainTypingName();

            cppTestHost.pressKey(window, Qt.Key_S, Qt.ControlModifier);

            compare(cppCollection.nodeName("r2"), "x");
            compare(cppWorkspaceManager.dirty, false);
            verify(findChild(window.contentItem, "nameField").activeFocus);
        }

        /*! Verifies that Ctrl+N keeps a name still being typed in its own request. */
        function test_mainNewShortcutStoresNameBeingEdited() {
            const window = createMainTypingName();

            cppTestHost.pressKey(window, Qt.Key_N, Qt.ControlModifier);

            compare(cppCollection.nodeName("r2"), "x");
            compare(cppRequestEditor.name, "New request");
        }

        /*! Verifies that the send shortcut works in the application window. */
        function test_mainSendShortcutSends() {
            const window = createMainTypingName(cppTestHost.serverUrl("/status/201"));

            cppTestHost.pressKey(window, Qt.Key_Return, Qt.ControlModifier);
            tryCompare(cppResponse, "busy", false, 10000);

            compare(cppResponse.statusCode, 201);
            compare(cppCollection.nodeName("r2"), "x");
        }

        /*! Verifies that an entry opened in the application window shows up as a request. */
        function test_mainOpeningEntryShowsRequest() {
            cppTestHost.addHistory("http://x/items", 1);
            const window = createTemporaryObject(mainComponent, root);
            verify(window !== null);
            waitForRendering(window.contentItem);

            const id = cppHistory.openEntry(0, cppCollection);
            findChild(window.contentItem, "historyList").parent.parent.entryOpened(id);

            compare(cppRequestEditor.url, "http://x/items");
        }

        /*! Creates the application window, active, and returns it. */
        function createMain() {
            const window = createTemporaryObject(mainComponent, root);
            verify(window !== null);
            window.requestActivate();
            tryCompare(window, "active", true);
            waitForRendering(window.contentItem);
            return window;
        }

        /*! Returns the help panel of \a window. */
        function helpPanel(window) {
            return findChild(window, "helpPanel");
        }

        /*! Verifies that every control of the application explains itself in both languages. */
        function test_everyControlHasHelp() {
            const window = createMain();
            for (const tabs of ["tabs", "responseTabs"]) {
                const bar = findChild(window.contentItem, tabs);
                for (let i = 0; i < bar.count; ++i) {
                    bar.currentIndex = i;
                    waitForRendering(window.contentItem);
                }
            }

            const missing = cppTestHost.controlsWithoutHelp(window);

            verify(missing.length === 0, missing.length + " without help:\n" + missing.join("\n"));
        }

        /*! Verifies that F1 explains the field being typed in. */
        function test_f1ExplainsFocusedField() {
            const window = createMain();
            findChild(window.contentItem, "tree").parent.selectNode("r2");
            findChild(findChild(window.contentItem, "urlField"), "editArea").forceActiveFocus();
            cppTestHost.pressKey(window, Qt.Key_End, Qt.NoModifier);

            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);

            const panel = helpPanel(window);
            tryCompare(panel, "opened", true);
            compare(findChild(panel, "helpTitle").text, "Address (URL)");
        }

        /*! Verifies that F1 explains a button under the pointer. */
        function test_f1ExplainsHoveredButton() {
            const window = createMain();
            const button = findChild(window.contentItem, "manageEnvironmentsButton");
            mouseMove(button, button.width / 2, button.height / 2);

            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);

            const panel = helpPanel(window);
            tryCompare(panel, "opened", true);
            compare(findChild(panel, "helpTitle").text, "Manage environments");
        }

        /*! Verifies that F1 shows a topic in the panel. */
        function test_panelShowsTopic() {
            const window = createMain();
            mouseMove(window.contentItem, window.width - 4, window.height - 30);

            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);

            const panel = helpPanel(window);
            tryCompare(panel, "opened", true);
            compare(findChild(panel, "helpTitle").text, "Getting started");
            verify(findChild(panel, "help_example").text.length > 0);
            compare(findChild(panel, "helpMore").text, "More in the guide");
        }

        /*! Verifies that the panel names the chapter More opens, since the viewer starts at page one. */
        function test_panelNamesGuideChapter() {
            const window = createMain();
            const button = findChild(window.contentItem, "manageEnvironmentsButton");
            mouseMove(button, button.width / 2, button.height / 2);

            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);

            const panel = helpPanel(window);
            tryCompare(panel, "opened", true);
            const chapter = findChild(panel, "helpChapter");
            compare(chapter.visible, true);
            compare(chapter.text, "Guide chapter: Variables and environments");
        }

        /*! Verifies that the Help menu opens the guide and both web links. */
        function test_helpMenuOffersGuideAndLinks() {
            verify(cppTestHost.setGuidePresent(true));
            const window = createMain();

            findChild(window, "guideMenuItem").triggered();
            findChild(window, "practiceApiMenuItem").triggered();
            findChild(window, "statusCodesMenuItem").triggered();

            const urls = cppTestHost.openedUrls();
            compare(urls.length, 3);
            verify(urls[0].startsWith("file:///"), urls[0]);
            verify(urls[0].endsWith("/help/ReqDeck-Guide.en.pdf"), urls[0]);
            compare(urls[1], "https://httpbin.org");
            compare(urls[2], "https://developer.mozilla.org/docs/Web/HTTP/Status");
        }

        /*! Verifies that More in the panel opens the guide. */
        function test_moreOpensGuide() {
            verify(cppTestHost.setGuidePresent(true));
            const window = createMain();
            mouseMove(window.contentItem, window.width - 4, window.height - 30);
            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);
            const panel = helpPanel(window);
            tryCompare(panel, "opened", true);

            mouseClick(findChild(panel, "helpMore"));

            compare(cppTestHost.openedUrls().length, 1);
            verify(cppTestHost.openedUrls()[0].endsWith("/help/ReqDeck-Guide.en.pdf"));
        }

        /*!
            Saves the screenshots of the beginner guide into the folder named by
            REQDECK_GUIDE_SHOTS, and is skipped without it. It sends the sample
            requests to their public servers, so it needs the internet.
        */
        function test_guideShots() {
            const folder = cppTestHost.guideShotsDir();
            if (folder.length === 0)
                skip("REQDECK_GUIDE_SHOTS is not set");
            verify(cppTestHost.openSamples());
            const window = createMain();
            window.width = 1180;
            window.height = 720;
            window.darkTheme = false;
            const tree = findChild(window.contentItem, "tree");
            tree.expandRecursively();
            const collection = tree.parent;
            const send = (id) => {
                collection.selectNode(id);
                cppRequestEditor.send();
                tryCompare(cppResponse, "busy", false, 20000);
                compare(cppResponse.statusCode >= 200 && cppResponse.statusCode < 300, true,
                        id + " " + cppResponse.errorText);
            };
            const save = (item, name) => {
                wait(700);
                verify(cppTestHost.saveGrab(item, folder + "/" + name), name);
            };

            send("placeholder-post");
            send("get");
            cppTestHost.addHistory("http://localhost:9/orders", 1, "Connection refused");
            send("placeholder-get");
            save(window.contentItem, "first-request.png");
            save(findChild(window.contentItem, "responsePane"), "response.png");

            mouseMove(findChild(window.contentItem, "urlField"), 40, 10);
            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);
            tryCompare(helpPanel(window), "opened", true);
            wait(700);
            const pane = findChild(window.contentItem, "requestPane");
            const left = pane.mapToItem(window.contentItem, 0, 0).x;
            const panel = helpPanel(window);
            verify(cppTestHost.saveGrabArea(window, Qt.rect(left, 0, window.width - left, panel.y + panel.height + 12),
                                            folder + "/help.png"));
            helpPanel(window).close();

            collection.selectNode("post");
            findChild(window.contentItem, "tabs").currentIndex = 2;
            save(findChild(window.contentItem, "requestPane"), "body.png");

            findChild(window.contentItem, "manageEnvironmentsButton").clicked();
            tryCompare(findChild(window, "environmentDialog"), "opened", true);
            save(window.contentItem, "environments.png");
            findChild(window, "environmentDialog").close();

            findChild(window.contentItem, "sidebarTabs").currentIndex = 1;
            save(window.contentItem, "history.png");
        }

        /*! Verifies that a missing guide is reported with its path instead of doing nothing. */
        function test_missingGuideIsReported() {
            const window = createMain();

            findChild(window, "guideMenuItem").triggered();

            compare(cppTestHost.openedUrls().length, 0);
            verify(window.statusMessage.startsWith("The guide was not found: "), window.statusMessage);
            verify(window.statusMessage.endsWith("ReqDeck-Guide.en.pdf"), window.statusMessage);
            compare(window.statusMessageLevel, 2);
        }

        /*! Verifies that the panel leaves the focus where it was. */
        function test_panelDoesNotTakeFocus() {
            const window = createMainTypingName();

            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);

            tryCompare(helpPanel(window), "opened", true);
            verify(findChild(window.contentItem, "nameField").activeFocus);
            cppTestHost.pressKey(window, Qt.Key_Y, Qt.NoModifier);
            compare(findChild(window.contentItem, "nameField").text, "xy");
        }

        /*! Verifies that F1 on the same element closes the panel again. */
        function test_f1OpensAndF1Closes() {
            const window = createMain();
            const panel = helpPanel(window);

            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);
            tryCompare(panel, "opened", true);
            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);

            tryCompare(panel, "visible", false);
        }

        /*! Verifies that Escape closes the panel. */
        function test_escClosesPanel() {
            const window = createMain();
            const panel = helpPanel(window);
            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);
            tryCompare(panel, "opened", true);

            cppTestHost.pressKey(window, Qt.Key_Escape, Qt.NoModifier);

            tryCompare(panel, "visible", false);
        }

        /*! Verifies help inside a modal dialog, and that Escape closes help first. */
        function test_f1WorksInModalDialog() {
            const window = createMain();
            findChild(window.contentItem, "manageEnvironmentsButton").clicked();
            const dialog = findChild(window, "environmentDialog");
            tryCompare(dialog, "opened", true);
            const list = findChild(dialog, "environmentList");
            mouseMove(list, list.width / 2, 20);

            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);

            const panel = helpPanel(window);
            tryCompare(panel, "opened", true);
            compare(findChild(panel, "helpTitle").text, "Environments");
            verify(panel.z > dialog.z);

            cppTestHost.pressKey(window, Qt.Key_Escape, Qt.NoModifier);
            tryCompare(panel, "visible", false);
            compare(dialog.visible, true);
            dialog.close();
        }

        /*! Verifies that F1 on another element shows its topic instead of closing. */
        function test_panelFollowsNextF1() {
            const window = createMain();
            findChild(window.contentItem, "manageEnvironmentsButton").clicked();
            const dialog = findChild(window, "environmentDialog");
            tryCompare(dialog, "opened", true);
            const list = findChild(dialog, "environmentList");
            mouseMove(list, list.width / 2, 20);
            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);
            const panel = helpPanel(window);
            tryCompare(panel, "opened", true);

            const add = findChild(dialog, "addEnvironment");
            mouseMove(add, add.width / 2, add.height / 2);
            cppTestHost.pressKey(window, Qt.Key_F1, Qt.NoModifier);

            compare(panel.opened, true);
            compare(findChild(panel, "helpTitle").text, "Add environment");
            dialog.close();
        }

        /*! Verifies that topic text is shown as text. */
        function test_panelMarkupStaysText() {
            const window = createMain();
            const panel = helpPanel(window);

            panel.topic = {title: "<b>x</b>", what: "<img src='http://x'>", why: "<i>y</i>"};
            panel.open();
            tryCompare(panel, "opened", true);

            compare(findChild(panel, "helpTitle").textFormat, Text.PlainText);
            compare(findChild(panel, "helpWhat").textFormat, Text.PlainText);
            compare(findChild(panel, "help_why").textFormat, Text.PlainText);
        }

        /*! Verifies that a list being read keeps its entry when a request finishes. */
        function test_historyKeepsCurrentEntry() {
            cppTestHost.addHistory("http://older", 1);
            cppTestHost.addHistory("http://newer", 1);
            const list = findChild(createHistory(), "historyList");
            list.forceActiveFocus();
            keyClick(Qt.Key_Down);
            keyClick(Qt.Key_Down);
            compare(list.currentIndex, 1);

            cppTestHost.addHistory("http://latest", 1);

            tryCompare(list, "currentIndex", 2);
            keyClick(Qt.Key_Return);
            compare(cppRequestEditor.url, "http://older");
        }

        /*! Verifies that a percent sign in a URL is not read as a placeholder. */
        function test_curlDialogKeepsPercentInSummary() {
            const host = openCurl();

            enterCommand(host, "curl --compressed 'http://x/a%2Fb?c=%1'");

            const status = findChild(host, "curlStatus").text;
            verify(status.indexOf("http://x/a%2Fb?c=%1") >= 0, status);
            verify(status.indexOf("--compressed") >= 0, status);
        }

        /*! Creates the items used by the help tests. */
        function createHelpTargets() {
            const targets = createTemporaryObject(helpTargetsComponent, root);
            verify(targets !== null);
            waitForRendering(targets);
            return targets;
        }

        /*! Returns the scene position of the middle of \a item. */
        function sceneCenter(item) {
            return item.mapToItem(null, item.width / 2, item.height / 2);
        }

        /*! Verifies that the innermost element with a topic wins. */
        function test_topicAtFindsDeepestElement() {
            const targets = createHelpTargets();

            compare(cppHelpLocator.topicAt(root.Window.window, sceneCenter(targets.field)), "field");
            compare(cppHelpLocator.topicAt(root.Window.window, targets.mapToItem(null, 300, 250)), "outer");
        }

        /*! Verifies that an element without a topic takes its parent's. */
        function test_topicAtWalksUpToParent() {
            const targets = createHelpTargets();

            compare(cppHelpLocator.topicAt(root.Window.window, sceneCenter(targets.plain)), "inner");
        }

        /*! Verifies that a disabled button still explains itself. */
        function test_topicAtFindsDisabledButton() {
            const targets = createHelpTargets();

            compare(cppHelpLocator.topicAt(root.Window.window, sceneCenter(targets.disabledButton)), "disabled");
        }

        /*! Verifies that elements in an open dialog are found above the screen. */
        function test_topicAtFindsItemInDialog() {
            const host = openDialog();
            const list = findChild(host, "environmentList");

            compare(cppHelpLocator.topicAt(root.Window.window, sceneCenter(list)), "environments.list");
        }

        /*! Verifies that the pointer decides after the mouse moved. */
        function test_pointerWinsAfterMouseMove() {
            const targets = createHelpTargets();
            targets.field.forceActiveFocus();
            keyClick(Qt.Key_A);

            mouseMove(targets.inner, 50, 50);

            compare(cppHelpLocator.currentTopic(root.Window.window), "inner");
        }

        /*! Verifies that the focus decides after a key press. */
        function test_focusWinsAfterKeyPress() {
            const targets = createHelpTargets();
            mouseMove(targets.inner, 50, 50);

            targets.field.forceActiveFocus();
            keyClick(Qt.Key_A);

            compare(cppHelpLocator.topicForFocus(root.Window.window), "field");
            compare(cppHelpLocator.currentTopic(root.Window.window), "field");
        }

        /*! Verifies that F1 itself does not count as using the keyboard. */
        function test_f1KeyDoesNotChangeTheChoice() {
            const targets = createHelpTargets();
            targets.field.forceActiveFocus();
            mouseMove(targets.inner, 50, 50);

            keyClick(Qt.Key_F1);

            compare(cppHelpLocator.currentTopic(root.Window.window), "inner");
        }

        /*! Verifies the general page when nothing has a topic. */
        function test_noTopicGivesStart() {
            const targets = createHelpTargets();
            mouseMove(root, root.width - 2, root.height - 2);

            compare(cppHelpLocator.currentTopic(root.Window.window), "start");
        }

        /*! Opens the environment dialog and waits until it is shown. */
        function openDialog() {
            const host = createTemporaryObject(dialogComponent, root);
            verify(host !== null);
            host.dialog.open();
            tryCompare(host.dialog, "opened", true);
            waitForRendering(findChild(host, "environmentList"));
            return host;
        }

        /*! Replaces the name in the dialog of \a host by the letter x, uncommitted. */
        function typeName(host) {
            const field = findChild(host, "environmentName");
            field.forceActiveFocus();
            field.selectAll();
            keyClick(Qt.Key_X);
        }

        /*! Returns the child \a name of the first row of the variable table. */
        function firstVariableChild(host, name) {
            const rows = findChild(findChild(host, "variablesTable"), "rows");
            return findChild(rows.itemAtIndex(0), name);
        }

        /*! Verifies that the corners of dialogs are not those of a phone. */
        function test_dialogsUseSmallCorners() {
            const host = openDialog();

            compare(host.dialog.Material.roundedScale, Material.SmallScale);
        }

        /*! Verifies that an empty Params tab explains itself. */
        function test_emptyParamsTabShowsHint() {
            const pane = createRequest("r2");

            compare(findChild(findChild(pane, "paramsTable"), "emptyHint").visible, true);
        }

        /*! Verifies that the dialog starts at the active environment. */
        function test_dialogStartsAtActiveEnvironment() {
            cppEnvironments.activeIndex = 1;

            const host = openDialog();

            compare(cppEnvironments.editedRow, 1);
            compare(findChild(host, "environmentName").text, "Missing");
            compare(firstVariableChild(host, "keyField").text, "baseUrl");
        }

        /*! Verifies that a secret value is masked and stays in the table. */
        function test_secretValueIsMasked() {
            const host = openDialog();
            const value = firstVariableChild(host, "valueField");
            const box = firstVariableChild(host, "secretBox");
            verify(box.visible);
            compare(value.echoMode, TextInput.Normal);
            const text = value.text;

            mouseClick(box);

            compare(value.echoMode, TextInput.Password);
            compare(value.text, text);
        }

        /*! Verifies that the table gets the room the list does not need. */
        function test_variableFieldsHaveRoom() {
            const host = openDialog();

            compare(findChild(host, "environmentList").width, 220);
            verify(firstVariableChild(host, "valueField").width > 120);
        }

        /*! Verifies that a row added in the dialog reaches the workspace. */
        function test_addedVariableReachesWorkspace() {
            const host = openDialog();

            mouseClick(findChild(findChild(host, "variablesTable"), "addButton"));

            compare(cppEnvironments.variablesModel.rowCount(), 2);
            verify(cppWorkspaceManager.dirty);
        }

        /*! Verifies that names from a file cannot fetch anything. */
        function test_environmentNameStaysText() {
            const requests = cppTestHost.requestCount();
            const name = '<img src="' + cppTestHost.serverUrl("/x") + '">';
            verify(cppEnvironments.renameEnvironment(0, name));

            const selector = createSelector();
            selector.popup.open();
            tryCompare(selector.popup, "opened", true);
            waitForRendering(selector);
            selector.popup.close();
            tryCompare(selector.popup, "visible", false);

            const host = openDialog();
            mouseClick(findChild(host, "removeEnvironment"));
            const confirmation = findChild(host, "removeConfirmation");
            tryCompare(confirmation, "opened", true);
            waitForRendering(findChild(host, "environmentList"));
            wait(200);
            confirmation.reject();

            compare(cppTestHost.requestCount(), requests);
        }

        /*! Verifies that choosing another row stores the name being typed. */
        function test_renameCommitsWhenSelectingAnother() {
            const host = openDialog();
            typeName(host);

            mouseClick(findChild(host, "environmentList").itemAtIndex(1));

            compare(cppEnvironments.names[0], "x");
            compare(cppEnvironments.editedRow, 1);
            compare(findChild(host, "environmentName").text, "Missing");
        }

        /*! Verifies that the Close button stores the name being typed. */
        function test_renameCommitsOnClose() {
            const host = openDialog();
            typeName(host);

            mouseClick(host.dialog.standardButton(Dialog.Close));

            tryCompare(host.dialog, "visible", false);
            compare(cppEnvironments.names[0], "x");
        }

        /*! Verifies that Escape stores the name being typed. */
        function test_renameCommitsOnEscape() {
            const host = openDialog();
            typeName(host);

            keyClick(Qt.Key_Escape);

            tryCompare(host.dialog, "visible", false);
            compare(cppEnvironments.names[0], "x");
        }

        /*! Verifies that Tab reaches the list and the arrows choose a row. */
        function test_listWorksByKeyboard() {
            const host = openDialog();
            const list = findChild(host, "environmentList");

            findChild(host, "environmentName").forceActiveFocus();
            for (let step = 0; step < 12 && !list.activeFocus; ++step)
                keyClick(Qt.Key_Tab);
            verify(list.activeFocus);
            verify(findChild(host, "listFocusFrame").visible);

            keyClick(Qt.Key_Down);
            compare(cppEnvironments.editedRow, 1);
            compare(findChild(host, "environmentName").text, "Missing");

            keyClick(Qt.Key_Down);
            compare(cppEnvironments.editedRow, 1);

            keyClick(Qt.Key_Up);
            keyClick(Qt.Key_Up);
            compare(cppEnvironments.editedRow, 0);
        }

        /*! Verifies that the list hint sits inside an empty list. */
        function test_emptyListShowsHint() {
            cppEnvironments.removeEnvironment(0);
            cppEnvironments.removeEnvironment(0);

            const host = openDialog();
            const hint = findChild(host, "emptyLabel");
            const list = findChild(host, "environmentList");

            verify(hint.visible);
            verify(hint.parent === list);
        }

        /*! Verifies that variables cannot change while their values are unread. */
        function test_lockedSecretsMakeVariablesReadOnly() {
            verify(cppTestHost.reopenWithLockedSecrets());

            const host = openDialog();
            const table = findChild(host, "variablesTable");
            const note = findChild(host, "secretsNote");
            compare(table.enabled, false);
            compare(findChild(host, "duplicateEnvironment").enabled, false);
            verify(note.visible);

            host.dialog.close();
            tryCompare(host.dialog, "visible", false);
            verify(cppTestHost.unlockSecrets());

            host.dialog.open();
            tryCompare(host.dialog, "opened", true);
            compare(table.enabled, true);
            compare(findChild(host, "duplicateEnvironment").enabled, true);
            compare(note.visible, false);
        }

        /*! Verifies that a blank name gives way to the old one. */
        function test_blankNameIsRestored() {
            const host = openDialog();
            const field = findChild(host, "environmentName");
            field.forceActiveFocus();
            field.selectAll();
            keyClick(Qt.Key_Delete);
            compare(field.text, "");

            keyClick(Qt.Key_Return);

            compare(field.text, "Created");
            compare(cppEnvironments.names[0], "Created");
        }

        /*! Verifies that removing waits for a confirmation. */
        function test_removeAsksFirst() {
            const host = openDialog();
            const confirmation = findChild(host, "removeConfirmation");

            mouseClick(findChild(host, "removeEnvironment"));

            tryCompare(confirmation, "opened", true);
            compare(cppEnvironments.names.length, 2);

            confirmation.accept();

            compare(cppEnvironments.names, ["Missing"]);
        }

        /*! Verifies a workspace without environments. */
        function test_emptyWorkspaceDialogIsUsable() {
            cppEnvironments.removeEnvironment(0);
            cppEnvironments.removeEnvironment(0);

            const host = openDialog();
            const table = findChild(host, "variablesTable");
            const name = findChild(host, "environmentName");
            compare(cppEnvironments.editedRow, -1);
            compare(table.enabled, false);
            compare(name.enabled, false);

            mouseClick(findChild(host, "addEnvironment"));

            compare(cppEnvironments.editedRow, 0);
            compare(table.enabled, true);
            verify(name.activeFocus);
            compare(name.selectedText, "New environment");
        }

        /*! Verifies that a copy opens in the dialog. */
        function test_duplicateShowsTheCopy() {
            const host = openDialog();

            mouseClick(findChild(host, "duplicateEnvironment"));

            compare(cppEnvironments.editedRow, 2);
            compare(findChild(host, "environmentName").text, "Created copy");
        }

        /*! Verifies that a closed dialog holds on to nothing. */
        function test_closeForgetsEditedRow() {
            const host = openDialog();

            host.dialog.close();

            tryCompare(host.dialog, "visible", false);
            compare(cppEnvironments.editedRow, -1);
        }

        /*! Creates the wired environment selector with the focus. */
        function createSelector() {
            const selector = createTemporaryObject(selectorComponent, root);
            verify(selector !== null);
            selector.forceActiveFocus();
            return selector;
        }

        /*! Verifies that the selector offers no environment first. */
        function test_selectorShowsNoneFirst() {
            const selector = createSelector();

            compare(selector.count, 3);
            compare(selector.currentIndex, 0);
            compare(selector.displayText, "No environment");
        }

        /*! Verifies that the selector shows an environment activated elsewhere. */
        function test_selectorFollowsActiveIndex() {
            const selector = createSelector();

            cppEnvironments.activeIndex = 1;

            compare(selector.currentText, "Missing");
        }

        /*! Verifies that a changed list keeps the active environment selected. */
        function test_selectorKeepsSelectionWhenNamesChange() {
            const selector = createSelector();
            cppEnvironments.activeIndex = 1;

            verify(cppEnvironments.renameEnvironment(1, "Renamed"));
            compare(selector.currentIndex, 2);
            compare(selector.currentText, "Renamed");

            cppEnvironments.addEnvironment();
            compare(selector.currentText, "Renamed");

            cppEnvironments.removeEnvironment(0);
            compare(cppEnvironments.activeIndex, 0);
            compare(selector.currentIndex, 1);
            compare(selector.currentText, "Renamed");
        }

        /*! Verifies that removing the active environment shows none. */
        function test_selectorShowsNoneAfterRemoval() {
            const selector = createSelector();
            cppEnvironments.activeIndex = 1;

            cppEnvironments.removeEnvironment(1);

            compare(selector.currentIndex, 0);
            compare(selector.displayText, "No environment");
        }

        /*! Verifies the rows the selector reports, without its first row. */
        function test_selectorEmitsIndexWithoutNoneRow() {
            createSelector();

            keyClick(Qt.Key_Down);
            compare(cppEnvironments.activeIndex, 0);

            keyClick(Qt.Key_Down);
            compare(cppEnvironments.activeIndex, 1);

            keyClick(Qt.Key_Up);
            keyClick(Qt.Key_Up);
            compare(cppEnvironments.activeIndex, -1);
        }

        /*! Verifies that the selector decides where a request goes. */
        function test_switchingEnvironmentChangesTarget() {
            const host = createSender("{{baseUrl}}");
            const selector = createSelector();

            keyClick(Qt.Key_Down);
            sendAndWait(host);
            compare(cppResponse.statusCode, 201);

            selector.forceActiveFocus();
            keyClick(Qt.Key_Down);
            sendAndWait(host);
            compare(cppResponse.statusCode, 404);
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
            compare(findChild(host.response, "bodyArea").text, "status 201");
        }

        /*! Sends to a JSON list of \a rows rows and returns the wired panes. */
        function createPrettyBody(rows) {
            cppResponse.pretty = true;
            const host = createSender(cppTestHost.serverUrl("/json/" + rows));
            sendAndWait(host);
            return host;
        }

        /*! Verifies that the view can be switched from the keyboard. */
        function test_prettyControlWorksByKeyboard() {
            const host = createPrettyBody(3);
            const toggle = findChild(host.response, "prettySwitch");

            findChild(toggle, "option1").forceActiveFocus();
            keyClick(Qt.Key_Space);
            compare(cppResponse.pretty, false);

            findChild(toggle, "option0").forceActiveFocus();
            keyClick(Qt.Key_Space);
            compare(cppResponse.pretty, true);
        }

        /*! Verifies that the numbers stay on their lines while scrolling. */
        function test_lineNumbersFollowScroll() {
            const host = createPrettyBody(300);
            const area = findChild(host.response, "bodyArea");
            const numbers = findChild(host.response, "lineNumbers");
            verify(numbers.visible);
            compare(numbers.lineCount, area.lineCount);

            const line = 500;
            numbers.view.contentY = line * numbers.lineHeight;
            waitForRendering(numbers);

            verify(Math.abs(numbers.firstLine - line) <= 3, "first " + numbers.firstLine);
            const position = area.text.split("\n").slice(0, line).join("\n").length + 1;
            const textY = area.positionToRectangle(position).y - numbers.view.contentY;
            verify(Math.abs(numbers.lineY(line) - textY) <= 1,
                   numbers.lineY(line) + " against " + textY);
        }

        /*! Verifies that a long body only builds the visible numbers. */
        function test_lineNumbersStayFastOnLargeBody() {
            const host = createPrettyBody(25000);
            const numbers = findChild(host.response, "lineNumbers");
            verify(numbers.lineCount >= 15000, "lines " + numbers.lineCount);
            verify(numbers.visibleLines < 200, "built " + numbers.visibleLines);

            const started = Date.now();
            for (let step = 1; step <= 20; ++step) {
                numbers.view.contentY = step * 40 * numbers.lineHeight;
                wait(0);
            }

            const elapsed = Date.now() - started;
            verify(elapsed < 3000, "took " + elapsed + " ms");
        }

        /*! Verifies that a wrapped body shows no numbers: its lines are not rows. */
        function test_lineNumbersHiddenWhenWrapping() {
            const host = createPrettyBody(3);
            const toggle = findChild(host.response, "prettySwitch");

            mouseClick(findChild(toggle, "option1"));

            compare(findChild(host.response, "lineNumbers").visibleLines, 0);
        }

        /*! Verifies that the numbers survive a theme switch. */
        function test_lineNumbersSurviveThemeSwitch() {
            const host = createPrettyBody(30);
            const numbers = findChild(host.response, "lineNumbers");
            const count = numbers.lineCount;

            BsTheme.dark = !BsTheme.dark;
            waitForRendering(numbers);
            BsTheme.dark = !BsTheme.dark;
            waitForRendering(numbers);

            compare(numbers.lineCount, count);
            verify(numbers.lineHeight > 0);
        }

        /*! Verifies that the owner of a response is told by identity, not by name. */
        function test_responseLabelUsesRequestIdentity() {
            const host = createPrettyBody(3);
            const label = findChild(host.response, "requestNameLabel");
            verify(cppCollection.rename("r1", "List items"));

            cppRequestEditor.load("r1");

            compare(cppRequestEditor.name, "List items");
            compare(label.visible, true);
        }

        /*! Verifies that many short lines show as fast as without numbers. */
        function test_shortLinesStayFast() {
            cppResponse.pretty = true;
            const host = createSender(cppTestHost.serverUrl("/lines/30000"));
            const started = Date.now();

            sendAndWait(host);

            const elapsed = Date.now() - started;
            const numbers = findChild(host.response, "lineNumbers");
            compare(numbers.lineCount, 30002);
            verify(elapsed < 9000, "took " + elapsed + " ms");
        }

        /*! Verifies that the numbers meet their lines after the view changed. */
        function test_lineNumbersAlignAfterSwitches() {
            const host = createPrettyBody(100);
            const area = findChild(host.response, "bodyArea");
            const numbers = findChild(host.response, "lineNumbers");
            const toggle = findChild(host.response, "prettySwitch");

            mouseClick(findChild(toggle, "option1"));
            waitForRendering(numbers);
            mouseClick(findChild(toggle, "option0"));
            BsTheme.dark = !BsTheme.dark;
            waitForRendering(numbers);
            BsTheme.dark = !BsTheme.dark;
            waitForRendering(numbers);
            numbers.view.contentY = 100 * numbers.lineHeight;
            waitForRendering(numbers);

            const line = 120;
            const position = area.text.split("\n").slice(0, line).join("\n").length + 1;
            const textY = area.positionToRectangle(position).y - numbers.view.contentY;
            verify(Math.abs(numbers.lineY(line) - textY) <= 1,
                   numbers.lineY(line) + " against " + textY);
        }

        /*! Verifies that the name shows only for a response of another request. */
        function test_requestNameHiddenForOpenRequest() {
            const host = createPrettyBody(3);
            const label = findChild(host.response, "requestNameLabel");
            compare(label.visible, false);

            cppRequestEditor.load("r1");

            compare(label.visible, true);
            compare(label.text, "List items");
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
            compare(toggle.currentIndex, 0);
            verify(area.text.indexOf("\n") >= 0);

            mouseClick(findChild(toggle, "option1"));

            compare(toggle.currentIndex, 1);
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
