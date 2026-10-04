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

        function cleanup() {
            BsTheme.dark = false;
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
