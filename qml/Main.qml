// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import Base as Base

/*!
    \qmltype Main
    \inqmlmodule ReqDeck
    \brief Provides the main application window.

    The window owns the top-level Material theme state and switches between the
    launcher (shown when no workspace is open) and the workspace (MainScreen). It
    hosts the workspace file dialogs, the unsaved-changes guard, and the
    settings, About, and update dialogs opened from menu actions.
*/
ApplicationWindow {
    id: mainWindow

    /*! Whether the application currently uses the dark Material theme. */
    property bool darkTheme: Application.styleHints.colorScheme === Qt.Dark

    /*! Pending action deferred until the unsaved-changes prompt is answered. */
    property var pendingAction: null

    /*! Whether the collapsible log panel is shown in the workspace. */
    property bool logPanelVisible: false

    /*! Text of the most recent operation outcome, shown in the status bar. */
    property string statusMessage: ""

    /*! Severity of \l statusMessage as a Diagnostics::Level value. */
    property int statusMessageLevel: 1

    /*!
        Stores a name or timeout still being edited: a shortcut leaves the focus
        where it is, unlike a click on a menu.
    */
    function commitFocusedEdit() {
        const focused = mainWindow.activeFocusItem
        editFocusSink.forceActiveFocus(Qt.ShortcutFocusReason)
        if (focused)
            focused.forceActiveFocus(Qt.ShortcutFocusReason)
    }

    /*!
        Opens help for the element F1 refers to, or closes it when that element's
        help is already shown.
    */
    function toggleHelp() {
        let id = cppHelpLocator.currentTopic(mainWindow)
        if (!cppHelp.hasTopic(id))
            id = "start"
        if (helpPanel.opened && helpPanel.topicId === id) {
            helpPanel.close()
            return
        }
        helpPanel.topicId = id
        helpPanel.topic = cppHelp.topic(id)
        helpPanel.open()
    }

    /*!
        Opens the beginner guide in the system PDF viewer, or says where it was
        looked for when it is missing. A viewer opened by the system starts at the
        first page, so \a chapter only names the chapter the help panel shows.
    */
    function openGuide(chapter) {
        if (!cppHelp.hasGuide()) {
            mainWindow.showNotification(2, // Diagnostics::Warning
                                        qsTr("The guide was not found: %1").arg(cppHelp.guidePath()))
            return
        }
        if (!Qt.openUrlExternally(cppHelp.guideUrl()))
            mainWindow.showNotification(2, // Diagnostics::Warning
                                        qsTr("No program to open PDF files was found: %1").arg(cppHelp.guidePath()))
    }

    /*! Opens the web address \a key of the help links in the browser. */
    function openHelpLink(key) {
        const url = cppHelp.links()[key]
        if (url)
            Qt.openUrlExternally(url)
    }

    /*! Saves the workspace with everything the user has typed so far. */
    function saveWorkspace() {
        mainWindow.commitFocusedEdit()
        cppWorkspaceManager.saveWorkspace()
    }

    /*!
        Records the outcome \a message at severity \a level.

        Every outcome lands in the status bar; the banner additionally surfaces
        warnings and errors, which the user has to notice.
    */
    function showNotification(level, message) {
        mainWindow.statusMessage = message
        mainWindow.statusMessageLevel = level
        mainScreen.notificationBanner.show(level, message)
    }

    /*!
        Updates the persistent status bar with \a message at \a level after a live
        language switch. Unlike showNotification() this does not re-raise the
        transient banner.
    */
    function refreshStatus(level, message) {
        mainWindow.statusMessage = message
        mainWindow.statusMessageLevel = level
    }

    /*!
        Runs \a action immediately, or, when the open workspace has unsaved
        changes, defers it behind the unsaved-changes prompt.
    */
    function runGuarded(action) {
        if (cppWorkspaceManager.dirty) {
            mainWindow.pendingAction = action
            unsavedDialog.open()
        } else {
            action()
        }
    }

    /*! Runs and clears the deferred action stored by runGuarded(). */
    function proceedPending() {
        const action = mainWindow.pendingAction
        mainWindow.pendingAction = null
        if (action)
            action()
    }

    width: 1200
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: cppWorkspaceManager.hasActiveWorkspace
           ? qsTr("%1 — %2%3")
                 .arg(cppAppInfo.appName)
                 .arg(cppWorkspaceManager.activeWorkspaceName)
                 .arg(cppWorkspaceManager.dirty ? "*" : "")
           : cppAppInfo.appName

    Material.theme: darkTheme ? Material.Dark : Material.Light
    Material.accent: Base.BsTheme.accent
    Material.primary: Material.BlueGrey

    // Guard application exit: prompt to save when the open workspace is dirty.
    onClosing: (close) => {
        // A field being edited commits when it loses the focus, and that edit
        // must count before asking about unsaved changes.
        mainWindow.contentItem.forceActiveFocus()
        if (cppWorkspaceManager.dirty) {
            close.accepted = false
            runGuarded(() => Qt.quit())
        }
    }

    Component.onCompleted: {
        if (cppUpdate.featureEnabled && cppUpdate.checkAutomatically)
            cppUpdate.checkNow()
    }

    // The shared chrome palette and the accent tokens follow the window theme,
    // including the View menu switch.
    Binding {
        target: Base.BsTheme
        property: "dark"
        value: mainWindow.darkTheme
    }

    // The workspace is present but hidden until a workspace is open, so its menu
    // bindings stay wired across open/close.
    MainScreen {
        id: mainScreen

        anchors.fill: parent
        visible: cppWorkspaceManager.hasActiveWorkspace
        darkTheme: mainWindow.darkTheme
        logPanelVisible: mainWindow.logPanelVisible
        appName: cppAppInfo.appName
        hasWorkspace: cppWorkspaceManager.hasActiveWorkspace
        workspaceDirty: cppWorkspaceManager.dirty
        hasPreviousCopy: cppWorkspaceManager.hasPreviousCopy
        recentWorkspaces: cppWorkspaceManager.recentWorkspaces
        environmentNames: cppEnvironments.names
        activeEnvironmentIndex: cppEnvironments.activeIndex
        hasRequest: cppRequestEditor.hasRequest
        canSend: cppRequestEditor.canSend
        canCancel: cppRequestEditor.busy
    }

    // The status bar belongs to the workspace; the launcher has nothing to report.
    footer: Base.BsStatusBar {
        visible: cppWorkspaceManager.hasActiveWorkspace
        workspaceName: cppWorkspaceManager.activeWorkspaceName
        workspaceDirty: cppWorkspaceManager.dirty
        message: mainWindow.statusMessage
        messageLevel: mainWindow.statusMessageLevel
        logPanelVisible: mainWindow.logPanelVisible

        onLogToggleRequested: mainWindow.logPanelVisible = !mainWindow.logPanelVisible
    }

    // The launcher is the entry point while no workspace is open.
    LauncherScreen {
        id: launcherScreen

        anchors.fill: parent
        visible: !cppWorkspaceManager.hasActiveWorkspace
        darkTheme: mainWindow.darkTheme

        onOpenWorkspaceRequested: openWorkspaceDialog.open()
        onCreateWorkspaceRequested: newWorkspaceDialog.open()
    }

    Connections {
        target: mainScreen.logPanel

        function onCloseRequested() {
            mainWindow.logPanelVisible = false
        }
    }

    Connections {
        target: mainScreen.menuBar

        function onNewWorkspaceRequested() {
            mainWindow.runGuarded(() => newWorkspaceDialog.open())
        }

        function onOpenWorkspaceRequested() {
            mainWindow.runGuarded(() => openWorkspaceDialog.open())
        }

        function onOpenRecentRequested(index) {
            mainWindow.runGuarded(() => cppWorkspaceManager.openRecent(index))
        }

        function onSaveWorkspaceRequested() {
            mainWindow.saveWorkspace()
        }

        function onSaveWorkspaceAsRequested() {
            saveWorkspaceDialog.open()
        }

        function onCloseWorkspaceRequested() {
            mainWindow.runGuarded(() => cppWorkspaceManager.closeWorkspace())
        }

        function onRestorePreviousCopyRequested() {
            restoreCopyDialog.open()
        }

        function onCopyCurlRequested() {
            cppRequestEditor.copyAsCurl()
        }

        function onImportCurlRequested() {
            curlImportDialog.parentId = cppCollection.parentForNewItem(mainScreen.collectionPane.selectedId)
            curlImportDialog.open()
        }

        function onSettingsRequested() {
            settingsDialog.open()
        }

        function onQuitRequested() {
            mainWindow.close()
        }

        function onThemeToggleRequested() {
            mainWindow.darkTheme = !mainWindow.darkTheme
        }

        function onLogPanelToggleRequested() {
            mainWindow.logPanelVisible = !mainWindow.logPanelVisible
        }

        function onCheckForUpdatesRequested() {
            cppUpdate.checkNow()
            updateDialog.open()
        }

        function onGuideRequested() {
            mainWindow.openGuide("")
        }

        function onPracticeApiRequested() {
            mainWindow.openHelpLink("practice")
        }

        function onStatusCodesRequested() {
            mainWindow.openHelpLink("statusCodes")
        }

        function onAboutRequested() {
            aboutDialog.open()
        }

        function onNewRequestRequested() {
            mainScreen.collectionPane.addRequest()
        }

        function onNewFolderRequested() {
            mainScreen.collectionPane.addFolder()
        }

        function onDuplicateRequested() {
            mainScreen.collectionPane.duplicateSelected()
        }

        function onSendRequested() {
            // A field being edited commits when it loses the focus.
            mainWindow.contentItem.forceActiveFocus()
            cppRequestEditor.send()
        }

        function onCancelRequested() {
            cppRequestEditor.cancel()
        }
    }

    // F1 works in dialogs too, so the shortcut is not limited to the window.
    Shortcut {
        sequence: "F1"
        context: Qt.ApplicationShortcut
        // Holding the key must not open and close the panel over and over.
        autoRepeat: false
        onActivated: mainWindow.toggleHelp()
    }

    // Escape closes help first, then whatever is below it.
    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        // Visible, not opened: the panel counts as opened only after it faded in.
        enabled: helpPanel.visible
        onActivated: helpPanel.close()
    }

    Base.BsHelpPanel {
        id: helpPanel

        /*! Topic of the F1 help. */
        property string helpTopic: "help.panel"

        /*! Id of the shown topic. */
        property string topicId: ""

        labels: cppHelp.labels()
        onMoreRequested: (chapter) => mainWindow.openGuide(chapter)
    }

    // Takes the focus in commitFocusedEdit(). It is a plain item on purpose: a focus
    // scope would hand the focus straight back to the field being edited.
    Item {
        id: editFocusSink
    }

    Shortcut {
        sequence: "Ctrl+N"
        enabled: cppWorkspaceManager.hasActiveWorkspace
        onActivated: {
            mainWindow.commitFocusedEdit()
            mainScreen.collectionPane.addRequest()
        }
    }

    Shortcut {
        sequence: "Ctrl+O"
        onActivated: {
            mainWindow.commitFocusedEdit()
            mainWindow.runGuarded(() => openWorkspaceDialog.open())
        }
    }

    Shortcut {
        sequence: "Ctrl+S"
        enabled: cppWorkspaceManager.hasActiveWorkspace
        onActivated: mainWindow.saveWorkspace()
    }

    Shortcut {
        sequence: "Ctrl+Shift+S"
        enabled: cppWorkspaceManager.hasActiveWorkspace
        onActivated: {
            mainWindow.commitFocusedEdit()
            saveWorkspaceDialog.open()
        }
    }

    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: mainWindow.close()
    }

    Connections {
        target: cppResponse

        function onBodySaved(path) {
            mainWindow.showNotification(1, // Diagnostics::Info
                                        qsTr("Saved the response to %1.").arg(path))
        }

        function onBodySaveFailed(message) {
            mainWindow.showNotification(2, message) // Diagnostics::Warning
        }
    }

    Connections {
        target: cppRequestEditor

        function onCopiedToClipboard() {
            mainWindow.showNotification(1, // Diagnostics::Info
                                        qsTr("Copied the request as a cURL command."))
        }
    }

    Connections {
        target: mainScreen.historyPane

        function onEntryOpened(id) {
            mainScreen.collectionPane.finishAdding(id)
        }
    }

    Connections {
        target: mainScreen.collectionPane

        function onNodeSelected(id) {
            cppRequestEditor.load(id)
        }

        function onDepthLimitReached() {
            mainWindow.showNotification(2, // Diagnostics::Warning
                                        qsTr("Folders cannot be nested more than 64 levels deep."))
        }
    }

    Connections {
        target: mainScreen.topActions

        function onEnvironmentSelected(index) {
            cppEnvironments.activeIndex = index
        }

        function onManageEnvironmentsRequested() {
            environmentDialog.open()
        }

        function onSaveWorkspaceRequested() {
            mainWindow.saveWorkspace()
        }

        function onSettingsRequested() {
            settingsDialog.open()
        }
    }

    Connections {
        target: cppWorkspaceManager

        function onWorkspaceError(message) {
            workspaceErrorLabel.text = message
            workspaceErrorDialog.open()
        }

        function onNotification(level, message) {
            mainWindow.showNotification(level, message)
        }

        function onStatusRetranslated(level, message) {
            mainWindow.refreshStatus(level, message)
        }
    }

    // An automatic startup check surfaces the dialog only when a newer version is
    // found, so it never interrupts the user when the application is up to date.
    // Quitting happens only after the user explicitly started the Maintenance
    // Tool, so the update can replace the application files.
    Connections {
        target: cppUpdate

        function onStatusChanged() {
            if (cppUpdate.updateAvailable)
                updateDialog.open()
        }

        function onQuitRequested() {
            Qt.quit()
        }
    }

    FileDialog {
        id: openWorkspaceDialog

        title: qsTr("Open Workspace")
        currentFolder: Qt.resolvedUrl("file:///" + cppWorkspaceManager.defaultWorkspacesDir)
        nameFilters: [qsTr("ReqDeck workspaces (*.reqdeck)"), qsTr("All files (*)")]
        onAccepted: cppWorkspaceManager.openWorkspace(selectedFile)
    }

    FileDialog {
        id: saveWorkspaceDialog

        title: qsTr("Save Workspace As")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "reqdeck"
        currentFolder: Qt.resolvedUrl("file:///" + cppWorkspaceManager.defaultWorkspacesDir)
        nameFilters: [qsTr("ReqDeck workspaces (*.reqdeck)")]
        onAccepted: cppWorkspaceManager.saveWorkspaceAs(selectedFile)
    }

    // Guided create-workspace form: the user types a name, sees the resulting
    // <name>.reqdeck file and full path, and can change the destination folder.
    Dialog {
        id: newWorkspaceDialog

        /*! Topic of the F1 help. */
        property string helpTopic: "dialog.workspace"

        /*! Local path of the destination folder for the new workspace. */
        property string workspaceFolder: ""

        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 560)
        title: qsTr("Create New Workspace")
        modal: true
        Material.roundedScale: Material.SmallScale
        focus: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape

        onOpened: {
            newWorkspaceNameField.text = ""
            newWorkspaceDialog.workspaceFolder = cppWorkspaceManager.defaultWorkspacesDir
            newWorkspaceNameField.forceActiveFocus()
        }

        onAccepted: {
            const name = newWorkspaceNameField.text.trim()
            if (cppWorkspaceManager.createWorkspace(name, newWorkspaceDialog.workspaceFolder))
                cppWorkspaceManager.setDefaultWorkspacesDir(newWorkspaceDialog.workspaceFolder)
            else
                Qt.callLater(() => newWorkspaceDialog.open()) // failed (e.g. name taken) — reopen to fix
        }

        Component.onCompleted: {
            const okButton = standardButton(Dialog.Ok)
            if (okButton)
                okButton.enabled = Qt.binding(() =>
                    newWorkspaceNameField.text.trim().length > 0
                    && newWorkspaceDialog.workspaceFolder.length > 0)
        }

        GridLayout {
            width: parent.width
            columns: 3
            columnSpacing: 10
            rowSpacing: 8

            Label { text: qsTr("Name:"); Layout.preferredWidth: 110 }
            TextField {
                id: newWorkspaceNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                placeholderText: qsTr("Workspace name")
                onAccepted: newWorkspaceDialog.accept()
            }
            Item { Layout.preferredWidth: 100 }

            Label { text: qsTr("File:"); Layout.preferredWidth: 110 }
            Label {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                elide: Text.ElideMiddle
                text: (newWorkspaceNameField.text.trim().length > 0
                       ? newWorkspaceNameField.text.trim() : qsTr("<name>")) + ".reqdeck"
            }

            Label { text: qsTr("Location:"); Layout.preferredWidth: 110 }
            TextField {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                readOnly: true
                text: newWorkspaceDialog.workspaceFolder
            }
            Button {
                text: qsTr("Browse…")
                Layout.preferredWidth: 100
                Layout.preferredHeight: 44
                onClicked: newWorkspaceFolderDialog.open()
            }

            Label { text: qsTr("Full path:"); Layout.preferredWidth: 110 }
            Label {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                elide: Text.ElideMiddle
                opacity: 0.7
                text: newWorkspaceDialog.workspaceFolder + "/"
                      + (newWorkspaceNameField.text.trim().length > 0
                         ? newWorkspaceNameField.text.trim() : qsTr("<name>")) + ".reqdeck"
            }
        }
    }

    FolderDialog {
        id: newWorkspaceFolderDialog

        title: qsTr("Select Workspace Folder")
        currentFolder: Qt.resolvedUrl("file:///" + cppWorkspaceManager.defaultWorkspacesDir)
        onAccepted: newWorkspaceDialog.workspaceFolder = cppWorkspaceManager.toLocalPath(selectedFolder)
    }

    Dialog {
        id: unsavedDialog

        /*! Topic of the F1 help. */
        property string helpTopic: "dialog.confirm"

        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 460)
        title: qsTr("Unsaved changes")
        modal: true
        Material.roundedScale: Material.SmallScale
        focus: true
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape

        // Save the workspace, then continue only if the save succeeded.
        onAccepted: {
            if (cppWorkspaceManager.saveWorkspace())
                mainWindow.proceedPending()
            else
                mainWindow.pendingAction = null
        }
        // Discard changes and continue with the deferred action.
        onDiscarded: {
            unsavedDialog.close()
            mainWindow.proceedPending()
        }
        // Cancel abandons the deferred action.
        onRejected: mainWindow.pendingAction = null

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: qsTr("The workspace \"%1\" has unsaved changes. Save them before continuing?")
                      .arg(cppWorkspaceManager.activeWorkspaceName)
        }
    }

    Dialog {
        id: restoreCopyDialog

        /*! Topic of the F1 help. */
        property string helpTopic: "dialog.restoreCopy"

        objectName: "restoreCopyDialog"
        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 480)
        title: qsTr("Restore previous copy")
        modal: true
        Material.roundedScale: Material.SmallScale
        focus: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        closePolicy: Popup.CloseOnEscape

        onAccepted: cppWorkspaceManager.restorePreviousCopy()

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: qsTr("Put back workspace \"%1\" and its secret values as they were on %2, before their file format changed? All changes since then, saved or not, are lost.")
                      .arg(cppWorkspaceManager.activeWorkspaceName)
                      .arg(cppWorkspaceManager.previousCopyDate)
        }
    }

    Dialog {
        id: workspaceErrorDialog

        /*! Topic of the F1 help. */
        property string helpTopic: "dialog.confirm"

        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 480)
        title: qsTr("Workspace error")
        modal: true
        Material.roundedScale: Material.SmallScale
        focus: true
        standardButtons: Dialog.Ok
        closePolicy: Popup.CloseOnEscape

        Label {
            id: workspaceErrorLabel

            width: parent.width
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: settingsDialog

        /*! Topic of the F1 help. */
        property string helpTopic: "dialog.settings"

        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 620)
        title: qsTr("Settings")
        modal: true
        Material.roundedScale: Material.SmallScale
        focus: true
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape

        GridLayout {
            width: parent.width
            columns: 3
            columnSpacing: 10
            rowSpacing: 8

            // The language row appears once more than one UI language ships; the
            // choice applies live and is remembered for the next launch.
            Label {
                text: qsTr("Interface language:")
                Layout.preferredWidth: 180
                visible: settingsLanguageCombo.visible
            }
            Base.BsLanguageSelector {
                id: settingsLanguageCombo
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                visible: cppLocale.availableLanguages.length > 1
            }
            Item {
                Layout.preferredWidth: 100
                visible: settingsLanguageCombo.visible
            }

            Label { text: qsTr("Default workspaces folder:"); Layout.preferredWidth: 180 }
            TextField {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                readOnly: true
                text: cppWorkspaceManager.defaultWorkspacesDir
            }
            Button {
                text: qsTr("Browse…")
                Layout.preferredWidth: 100
                Layout.preferredHeight: 44
                onClicked: settingsFolderDialog.open()
            }

            // Automatic startup update check; disabled until the update feature is
            // enabled in the product configuration.
            Label { text: qsTr("Updates:"); Layout.preferredWidth: 180 }
            CheckBox {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                text: qsTr("Check for updates automatically on startup")
                enabled: cppUpdate.featureEnabled
                checked: cppUpdate.checkAutomatically
                onToggled: cppUpdate.checkAutomatically = checked
            }
        }
    }

    FolderDialog {
        id: settingsFolderDialog

        title: qsTr("Select Default Workspaces Folder")
        currentFolder: Qt.resolvedUrl("file:///" + cppWorkspaceManager.defaultWorkspacesDir)
        onAccepted: cppWorkspaceManager.setDefaultWorkspacesDir(selectedFolder)
    }

    EnvironmentDialog {
        id: environmentDialog

        width: Math.min(mainWindow.width - 80, 760)
        height: Math.min(mainWindow.height - 80, 520)
    }

    CurlImportDialog {
        id: curlImportDialog

        width: Math.min(mainWindow.width - 80, 680)
        height: Math.min(mainWindow.height - 80, 440)
        onImported: (id) => mainScreen.collectionPane.finishAdding(id)
    }

    AboutDialog {
        id: aboutDialog

        darkTheme: mainWindow.darkTheme
    }

    UpdateDialog {
        id: updateDialog

        darkTheme: mainWindow.darkTheme

        // Resolve unsaved workspace changes first (the user may cancel), then
        // start the Maintenance Tool; quitting follows through onQuitRequested.
        onInstallUpdateRequested: mainWindow.runGuarded(() => cppUpdate.installUpdate())
    }
}
