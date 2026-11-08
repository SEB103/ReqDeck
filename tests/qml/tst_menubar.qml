// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest
import Base

/*!
    \qmltype tst_menubar
    \brief Hosts QML smoke tests for the reusable Base components.

    Every component is created without the application's context; only a mock
    \c cppAppEngine (for the log panel) is injected by tst_qml.cpp.
*/
Item {
    id: root

    width: 1000
    height: 700

    Component {
        id: menuBarComponent

        BsMenuBar {}
    }

    Component {
        id: statusBarComponent

        BsStatusBar {
            width: 800
        }
    }

    Component {
        id: topActionsComponent

        BsTopBarActions {}
    }

    Component {
        id: logPanelComponent

        BsLogPanel {
            width: 800
            height: 200
        }
    }

    Component {
        id: bannerComponent

        BsNotificationBanner {
            width: 800
        }
    }

    Component {
        id: paneComponent

        BsPane {
            width: 300
            height: 200
            title: "Collection"
            placeholderText: "Nothing here yet."
        }
    }

    // Stands in for KeyValueModel: the table only needs its roles and two calls.
    ListModel {
        id: keyValueModel

        function addRow() {
            append({key: "", value: "", enabled: true});
        }

        function removeRow(row) {
            remove(row);
        }
    }

    Component {
        id: keyValueTableComponent

        BsKeyValueTable {
            width: 500
            height: 200
            model: keyValueModel
        }
    }

    Component {
        id: environmentSelectorComponent

        BsEnvironmentSelector {
            names: ["A", "B"]
            activeIndex: 1
        }
    }

    Component {
        id: segmentedComponent

        BsSegmentedControl {
            options: ["One", "Two"]
        }
    }

    Component {
        id: lineNumbersComponent

        BsLineNumbers {
            width: 40
            height: 100
        }
    }

    Component {
        id: singleLineEditComponent

        BsSingleLineEdit {
            width: 300
            text: "http://example.com"
        }
    }

    Component {
        id: statusChipComponent

        BsStatusChip {}
    }

    SignalSpy {
        id: checkForUpdatesSpy

        signalName: "checkForUpdatesRequested"
    }

    TestCase {
        id: testCase

        /*! Smoke test case for Base component creation and menu structure. */
        name: "QmlComponentSmoke"
        when: windowShown

        /*! Returns the menu of \a menuBar whose title, without mnemonics, is \a title. */
        function menuByTitle(menuBar, title) {
            for (let i = 0; i < menuBar.count; ++i) {
                const menu = menuBar.menuAt(i);
                if (menu.title.replace("&", "") === title)
                    return menu;
            }
            return null;
        }

        /*! Returns the item of \a menu whose text, without mnemonics, is \a text. */
        function itemByText(menu, text) {
            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i);
                if (item && item.text !== undefined && item.text.replace("&", "") === text)
                    return item;
            }
            return null;
        }

        /*! Fills the model with two rows, the second one disabled. */
        function resetKeyValueModel() {
            keyValueModel.clear();
            keyValueModel.append({key: "Accept", value: "*/*", enabled: true});
            keyValueModel.append({key: "X-Off", value: "1", enabled: false});
        }

        /*! Returns the child named \a name in the delegate of \a row of \a table. */
        function rowChild(table, row, name) {
            const rows = findChild(table, "rows");
            return findChild(rows.itemAtIndex(row), name);
        }

        function cleanup() {
            BsTheme.dark = false;
        }

        /*! Verifies that the table shows the rows of its model. */
        function test_menuLeavesRoomForShortcut_data() {
            return [{tag: "File", title: "File"}, {tag: "Request", title: "Request"}];
        }

        /*! Verifies that text, icon, and shortcut fit: test builds have no icons. */
        function test_menuLeavesRoomForShortcut(data) {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const menu = menuByTitle(menuBar, data.title);
            const iconWidth = 40;

            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i);
                if (item.shortcutText !== undefined && item.shortcutText.length > 0) {
                    verify(item.availableWidth >= item.contentItem.implicitWidth + iconWidth,
                           item.text + ": " + item.availableWidth);
                }
            }
        }

        /*! Returns the relative luminance of \a color. */
        function luminance(color) {
            const channel = (value) => value <= 0.03928 ? value / 12.92
                                                        : Math.pow((value + 0.055) / 1.055, 2.4);
            return 0.2126 * channel(color.r) + 0.7152 * channel(color.g) + 0.0722 * channel(color.b);
        }

        /*! Returns the contrast of \a color on \a tint laid over \a background. */
        function contrastOnTint(color, tint, background) {
            const mix = (channel) => tint.a * tint[channel] + (1 - tint.a) * background[channel];
            const under = Qt.rgba(mix("r"), mix("g"), mix("b"), 1);
            const first = luminance(color) + 0.05;
            const second = luminance(under) + 0.05;
            return Math.max(first, second) / Math.min(first, second);
        }

        function test_methodColorsAreReadable_data() {
            return [{tag: "light", dark: false, background: "#FFFBFE"},
                    {tag: "dark", dark: true, background: "#1C1B1F"}];
        }

        /*! Verifies that every method reads on the selected row in both themes. */
        function test_methodColorsAreReadable(data) {
            BsTheme.dark = data.dark;
            const background = Qt.color(data.background);

            for (const method of ["GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"]) {
                const color = Qt.color(BsTheme.methodColor(method));
                const selected = contrastOnTint(color, BsTheme.selectedRowColor, background);
                const hovered = contrastOnTint(color, Qt.alpha(BsTheme.accent, 0.06), background);
                verify(selected >= 3, method + ": " + selected);
                verify(hovered >= 3, method + ": " + hovered);
            }
            BsTheme.dark = false;
        }

        /*! Verifies that the header strip of a pane is compact. */
        function test_paneHeaderIsCompact() {
            const pane = createTemporaryObject(paneComponent, root);
            verify(pane !== null);

            compare(findChild(pane, "paneHeader").height, BsTheme.paneHeaderHeight);
            compare(BsTheme.paneHeaderHeight, 26);
            compare(BsTheme.compactBarHeight, 34);
        }

        /*! Verifies that the chip and the lists show a status in one color. */
        function test_statusChipUsesThemeColor() {
            const chip = createTemporaryObject(statusChipComponent, root);
            verify(chip !== null);

            chip.statusClass = 4;

            verify(Qt.colorEqual(chip.classColor, BsTheme.statusColor(404)));
        }

        /*! Verifies that assistive tools can see which option is chosen. */
        function test_segmentedControlExposesChoice() {
            const control = createTemporaryObject(segmentedComponent, root);
            verify(control !== null);

            compare(findChild(control, "option0").Accessible.checked, true);
            compare(findChild(control, "option1").Accessible.checked, false);
        }

        /*! Verifies that status colors follow the class of the code. */
        function test_statusColorByClass() {
            verify(Qt.colorEqual(BsTheme.statusColor(201), BsTheme.statusColor(204)));
            verify(!Qt.colorEqual(BsTheme.statusColor(404), BsTheme.statusColor(201)));
            verify(Qt.colorEqual(BsTheme.statusColor(0), BsTheme.statusColor(500)));
            verify(!Qt.colorEqual(BsTheme.statusColor(301), BsTheme.statusColor(404)));
        }

        /*! Verifies that a click reports the option and leaves the choice to the host. */
        function test_segmentedControlReportsChoice() {
            const control = createTemporaryObject(segmentedComponent, root);
            verify(control !== null);
            let chosen = -1;
            control.activated.connect((index) => chosen = index);

            mouseClick(findChild(control, "option1"));

            compare(chosen, 1);
            compare(control.currentIndex, 0);
        }

        /*! Verifies that the gutter is safe without a view. */
        function test_lineNumbersWithoutView() {
            const numbers = createTemporaryObject(lineNumbersComponent, root);
            verify(numbers !== null);

            compare(numbers.visibleLines, 0);
        }

        /*! Verifies that the menu items name their shortcuts. */
        function test_menuItemShowsShortcut() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);

            compare(itemByText(menuByTitle(menuBar, "Request"), "Send").shortcutText, "Ctrl+Enter");
            compare(itemByText(menuByTitle(menuBar, "Request"), "New Request").shortcutText, "Ctrl+N");
            compare(itemByText(menuByTitle(menuBar, "File"), "Save").shortcutText, "Ctrl+S");
            compare(itemByText(menuByTitle(menuBar, "File"), "Save As…").shortcutText, "Ctrl+Shift+S");
            compare(itemByText(menuByTitle(menuBar, "File"), "Open Workspace…").shortcutText, "Ctrl+O");
            compare(itemByText(menuByTitle(menuBar, "File"), "Quit").shortcutText, "Ctrl+Q");
        }

        /*! Verifies that Copy as cURL needs an open request. */
        function test_copyCurlFollowsHasRequest() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const item = itemByText(menuByTitle(menuBar, "Request"), "Copy as cURL");
            compare(item.enabled, false);

            menuBar.hasRequest = true;

            compare(item.enabled, true);
        }

        function test_environmentSelectorShowsActiveRow() {
            const selector = createTemporaryObject(environmentSelectorComponent, root);
            verify(selector !== null);
            compare(selector.currentText, "B");
        }

        function test_singleLineEditShowsItsText() {
            const edit = createTemporaryObject(singleLineEditComponent, root);
            verify(edit !== null);
            compare(edit.length, 18);
        }

        function test_keyValueTableShowsRows() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);

            compare(findChild(table, "rows").count, 2);
            compare(rowChild(table, 0, "keyField").text, "Accept");
            compare(rowChild(table, 0, "valueField").text, "*/*");
            compare(rowChild(table, 0, "enabledBox").checked, true);
            compare(rowChild(table, 1, "enabledBox").checked, false);
        }

        /*! Verifies that a long key or value shows its start while the field is not being edited. */
        function test_longValueShowsStart() {
            resetKeyValueModel();
            const long = "start-" + "x".repeat(300);
            keyValueModel.set(0, {key: long, value: long, enabled: true});
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);
            const value = rowChild(table, 0, "valueField");

            compare(rowChild(table, 0, "keyField").cursorPosition, 0);
            compare(value.cursorPosition, 0);

            value.forceActiveFocus();
            value.cursorPosition = value.length;
            rowChild(table, 1, "keyField").forceActiveFocus();
            compare(value.cursorPosition, 0);
        }

        /*! Verifies that typing and toggling reach the model. */
        function test_tableHidesSecretColumnByDefault() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);

            compare(rowChild(table, 0, "secretBox").visible, false);
            compare(rowChild(table, 0, "valueField").echoMode, TextInput.Normal);
        }

        /*! Verifies that the columns are named above the rows. */
        function test_tableShowsColumnTitles() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            table.keyTitle = "Header";
            table.valueTitle = "Content";

            compare(findChild(table, "keyColumnTitle").text, "Header");
            compare(findChild(table, "valueColumnTitle").text, "Content");
        }

        /*! Verifies that an empty table says what it is for. */
        function test_tableShowsHintWhenEmpty() {
            keyValueModel.clear();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            table.emptyHint = "Add one.";
            const hint = findChild(table, "emptyHint");
            compare(hint.visible, true);
            compare(hint.text, "Add one.");

            keyValueModel.addRow();

            tryCompare(hint, "visible", false);
        }

        /*! Verifies that a table without a hint shows none. */
        function test_tableWithoutHintShowsNone() {
            keyValueModel.clear();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);

            compare(findChild(table, "emptyHint").visible, false);
        }

        /*! Verifies that fields can still be told apart without floating labels. */
        function test_tableFieldsKeepNames() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);

            compare(rowChild(table, 1, "keyField").Accessible.name, "Key 2");
            compare(rowChild(table, 1, "valueField").Accessible.name, "Value 2");
        }

        /*! Verifies that no floating label can be cut off by the list. */
        function test_firstRowIsNotClipped() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);

            compare(rowChild(table, 0, "keyField").placeholderText, "");
            compare(rowChild(table, 0, "valueField").placeholderText, "");
        }

        /*! Verifies that the rows stay compact. */
        function test_tableRowsAreCompact() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);

            compare(rowChild(table, 0, "keyField").height, 34);
        }

        function test_keyValueTableEditsWriteToModel() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);

            const field = rowChild(table, 0, "valueField");
            field.forceActiveFocus();
            field.cursorPosition = field.length;
            keyClick(Qt.Key_X);
            compare(keyValueModel.get(0).value, "*/*x");

            mouseClick(rowChild(table, 1, "enabledBox"));
            compare(keyValueModel.get(1).enabled, true);
        }

        /*! Verifies that the remove button removes its row. */
        function test_keyValueTableRemoveCallsModel() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);

            mouseClick(rowChild(table, 0, "removeButton"));
            compare(keyValueModel.count, 1);
            compare(keyValueModel.get(0).key, "X-Off");
        }

        /*! Verifies that the add button appends a row. */
        function test_keyValueTableAddCallsModel() {
            resetKeyValueModel();
            const table = createTemporaryObject(keyValueTableComponent, root);
            verify(table !== null);
            waitForRendering(table);

            mouseClick(findChild(table, "addButton"));
            compare(keyValueModel.count, 3);
        }

        /*! Verifies that the chip shows its text. */
        function test_statusChipShowsText() {
            const chip = createTemporaryObject(statusChipComponent, root, {text: "200 OK", statusClass: 2});
            verify(chip !== null);
            compare(findChild(chip, "label").text, "200 OK");
            verify(chip.implicitWidth > 0);
        }

        /*! Verifies that each status class has its own color in both themes. */
        function test_statusChipColorFollowsClass() {
            const chip = createTemporaryObject(statusChipComponent, root);
            verify(chip !== null);

            for (const dark of [false, true]) {
                BsTheme.dark = dark;
                const colors = [];
                for (const statusClass of [2, 3, 4, 5]) {
                    chip.statusClass = statusClass;
                    colors.push(chip.classColor.toString());
                }
                compare(new Set(colors).size, 4);
            }
        }

        /*! Verifies when Send and Cancel are available. */
        function test_menuSendFollowsState() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const menu = menuByTitle(menuBar, "Request");
            const send = itemByText(menu, "Send");
            const cancel = itemByText(menu, "Cancel");

            compare(send.enabled, false);
            compare(cancel.enabled, false);

            menuBar.canSend = true;
            compare(send.enabled, true);
            menuBar.canSend = false;
            menuBar.canCancel = true;
            compare(send.enabled, false);
            compare(cancel.enabled, true);
        }

        /*! Verifies when the Request menu actions are available. */
        function test_menuRequestActionsFollowState() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const menu = menuByTitle(menuBar, "Request");
            const newRequest = itemByText(menu, "New Request");
            const newFolder = itemByText(menu, "New Folder");
            const duplicate = itemByText(menu, "Duplicate");

            compare(newRequest.enabled, false);
            compare(newFolder.enabled, false);
            compare(duplicate.enabled, false);

            menuBar.hasWorkspace = true;
            compare(newRequest.enabled, true);
            compare(newFolder.enabled, true);
            compare(duplicate.enabled, false);

            menuBar.hasSelection = true;
            compare(duplicate.enabled, true);
        }

        /*! Verifies that BsMenuBar can be created and its theme property is writable. */
        function test_menuBarCreationAndThemeProperty() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            compare(menuBar.darkTheme, false);
            menuBar.darkTheme = true;
            compare(menuBar.darkTheme, true);
        }

        /*! Verifies the File, Request, View, and Help menus in this order. */
        function test_menuBarTitles() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            compare(menuBar.count, 4);
            const titles = [];
            for (let i = 0; i < menuBar.count; ++i)
                titles.push(menuBar.menuAt(i).title.replace("&", ""));
            compare(titles, ["File", "Request", "View", "Help"]);
        }

        /*! Verifies that the menu bar exposes the update-check signal. */
        function test_menuBarCheckForUpdatesSignal() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            checkForUpdatesSpy.target = menuBar;
            verify(checkForUpdatesSpy.valid);
        }

        /*!
            Verifies that the menu bar titles are text-only while every dropdown
            still shows icons next to its items.
        */
        function test_menuBarTitlesHaveNoIcons() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            for (let i = 0; i < menuBar.count; ++i) {
                const menu = menuBar.menuAt(i);
                compare(menuBar.itemAt(i).icon.source.toString(), "",
                        "menu bar title " + menu.title + " shows an icon");

                let iconCount = 0;
                for (let j = 0; j < menu.count; ++j) {
                    const item = menu.itemAt(j);
                    if (item && item.icon !== undefined
                            && item.icon.source.toString().length > 0) {
                        ++iconCount;
                    }
                }
                verify(iconCount > 0, "menu " + menu.title + " has no item icons");
            }
        }

        /*!
            Verifies that submenu entries draw their icons at the same size as
            plain menu items. A submenu entry copies the whole icon from
            Menu.icon, so an unset size there falls back to the SVG's
            intrinsic size instead of the style's menu item icon size.
        */
        function test_menuItemIconsHaveUniformSize() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);

            const items = [];
            const collect = menu => {
                for (let i = 0; i < menu.count; ++i) {
                    const item = menu.itemAt(i);
                    if (!item || item.icon === undefined)
                        continue;
                    if (item.icon.source.toString().length > 0)
                        items.push(item);
                    if (item.subMenu)
                        collect(item.subMenu);
                }
            };
            for (let i = 0; i < menuBar.count; ++i)
                collect(menuBar.menuAt(i));

            const plainItem = items.find(item => !item.subMenu);
            verify(plainItem !== undefined);
            verify(items.some(item => item.subMenu), "no submenu entry with an icon found");
            verify(plainItem.icon.width > 0);

            for (const item of items) {
                compare(item.icon.width, plainItem.icon.width,
                        "icon width of \"" + item.text + "\"");
                compare(item.icon.height, plainItem.icon.height,
                        "icon height of \"" + item.text + "\"");
            }
        }

        /*! Verifies that the Request menu holds only disabled placeholder actions. */
        function test_requestMenuIsPlaceholder() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const requestMenu = menuByTitle(menuBar, "Request");
            verify(requestMenu !== null);

            let actionCount = 0;
            for (let i = 0; i < requestMenu.count; ++i) {
                const item = requestMenu.itemAt(i);
                if (!item || item.text === undefined || item.text.length === 0)
                    continue;
                ++actionCount;
                verify(!item.enabled, "request action " + item.text + " is enabled");
            }
            compare(actionCount, 6);
        }

        /*! Verifies that the workspace-scoped File items follow the workspace state. */
        function test_fileMenuFollowsWorkspaceState() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const fileMenu = menuByTitle(menuBar, "File");
            const save = itemByText(fileMenu, "Save");
            const saveAs = itemByText(fileMenu, "Save As…");
            const close = itemByText(fileMenu, "Close Workspace");
            verify(save !== null && saveAs !== null && close !== null);

            verify(!save.enabled && !saveAs.enabled && !close.enabled);

            menuBar.hasWorkspace = true;
            verify(!save.enabled);
            verify(saveAs.enabled && close.enabled);

            menuBar.workspaceDirty = true;
            verify(save.enabled);
        }

        /*! Verifies that Restore Previous Copy needs a workspace with a copy and asks through a signal. */
        function test_restorePreviousCopyFollowsState() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const restore = itemByText(menuBar.menuAt(0), "Restore Previous Copy…");
            verify(restore !== null);
            verify(!restore.enabled);

            menuBar.hasWorkspace = true;
            verify(!restore.enabled);
            menuBar.hasPreviousCopy = true;
            verify(restore.enabled);

            let asked = 0;
            menuBar.restorePreviousCopyRequested.connect(() => ++asked);
            restore.triggered();
            compare(asked, 1);
        }

        /*! Verifies that recent workspaces populate the Open Recent submenu. */
        function test_recentWorkspacesFillSubmenu() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const fileMenu = menuByTitle(menuBar, "File");
            const recentEntry = itemByText(fileMenu, "Open Recent");
            verify(recentEntry !== null && recentEntry.subMenu);
            verify(!recentEntry.enabled);

            menuBar.recentWorkspaces = [
                { displayName: "Billing", path: "C:/w/billing.reqdeck", available: true },
                { displayName: "", path: "C:/w/gone.reqdeck", available: false }
            ];
            verify(recentEntry.enabled);
            const recentMenu = recentEntry.subMenu;
            tryCompare(recentMenu, "count", 2);
            compare(recentMenu.itemAt(0).text, "Billing");
            compare(recentMenu.itemAt(1).text, "C:/w/gone.reqdeck");
            verify(!recentMenu.itemAt(1).enabled);
        }

        /*! Verifies the About item names the product when it is known. */
        function test_aboutItemUsesAppName() {
            const menuBar = createTemporaryObject(menuBarComponent, root);
            verify(menuBar !== null);
            const helpMenu = menuByTitle(menuBar, "Help");
            verify(itemByText(helpMenu, "About…") !== null);
            menuBar.appName = "ReqDeck";
            verify(itemByText(helpMenu, "About ReqDeck…") !== null);
        }

        /*! Verifies the accent tokens and their dark variants. */
        function test_themeAccentTokens() {
            verify(Qt.colorEqual(BsTheme.accent, "#C2185B"));
            verify(Qt.colorEqual(BsTheme.accentStrong, "#AD1457"));
            verify(Qt.colorEqual(BsTheme.accentSoft, "#F48FB1"));
            verify(Qt.colorEqual(BsTheme.accentText, "#AD1457"));
            verify(Qt.colorEqual(BsTheme.accentLineColor, BsTheme.accent));

            BsTheme.dark = true;
            verify(Qt.colorEqual(BsTheme.accent, "#F06292"));
            verify(Qt.colorEqual(BsTheme.accentText, "#F48FB1"));
            verify(Qt.colorEqual(BsTheme.statusLineColor(true), "#F06292"));
            verify(Qt.colorEqual(BsTheme.statusLineColor(false), BsTheme.dividerColor));
        }

        /*! Verifies that BsStatusBar is a pure display of the supplied state. */
        function test_statusBarCreation() {
            const statusBar = createTemporaryObject(statusBarComponent, root);
            verify(statusBar !== null);
            compare(statusBar.busy, false);
            statusBar.workspaceName = "Billing";
            statusBar.workspaceDirty = true;
            statusBar.message = "Saved workspace Billing.";
            statusBar.messageLevel = 3;
            verify(Qt.colorEqual(statusBar.messageColor, "#F44336"));
            verify(statusBar.implicitHeight > 0);
        }

        /*! Verifies that the quick-action Save follows canSave. */
        function test_topActionsCreation() {
            const actions = createTemporaryObject(topActionsComponent, root);
            verify(actions !== null);
            compare(actions.canSave, false);
            actions.canSave = true;
            compare(actions.canSave, true);
        }

        /*! Verifies that the log panel binds to the engine's filtered log. */
        function test_logPanelCreation() {
            cppAppEngine.clearLog();
            const panel = createTemporaryObject(logPanelComponent, root);
            verify(panel !== null);
            cppAppEngine.log(1, "first entry");
            cppAppEngine.log(3, "second entry");
            compare(cppAppEngine.logModel.count, 2);
            cppAppEngine.clearLog();
            compare(cppAppEngine.logModel.count, 0);
        }

        /*! Verifies that the banner shows warnings and errors only and can be dismissed. */
        function test_notificationBanner() {
            const banner = createTemporaryObject(bannerComponent, root);
            verify(banner !== null);
            verify(!banner.visible);

            banner.show(1, "routine progress");
            verify(!banner.visible);

            banner.show(3, "request failed");
            verify(banner.visible);
            verify(banner.persistent);

            banner.dismiss();
            verify(!banner.visible);
        }

        /*! Verifies that BsPane shows its placeholder while it has no content. */
        function test_paneCreation() {
            const pane = createTemporaryObject(paneComponent, root);
            verify(pane !== null);
            compare(pane.title, "Collection");
            compare(pane.placeholderText, "Nothing here yet.");
        }
    }
}
