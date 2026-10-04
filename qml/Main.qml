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
        recentWorkspaces: cppWorkspaceManager.recentWorkspaces
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
            cppWorkspaceManager.saveWorkspace()
        }

        function onSaveWorkspaceAsRequested() {
            saveWorkspaceDialog.open()
        }

        function onCloseWorkspaceRequested() {
            mainWindow.runGuarded(() => cppWorkspaceManager.closeWorkspace())
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

        function onAboutRequested() {
            aboutDialog.open()
        }
    }

    Connections {
        target: mainScreen.topActions

        function onSaveWorkspaceRequested() {
            cppWorkspaceManager.saveWorkspace()
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

        /*! Local path of the destination folder for the new workspace. */
        property string workspaceFolder: ""

        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 560)
        title: qsTr("Create New Workspace")
        modal: true
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

        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 460)
        title: qsTr("Unsaved changes")
        modal: true
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
            text: qsTr("The workspace \"%1\" has unsaved changes. Save them before continuing?")
                      .arg(cppWorkspaceManager.activeWorkspaceName)
        }
    }

    Dialog {
        id: workspaceErrorDialog

        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 480)
        title: qsTr("Workspace error")
        modal: true
        focus: true
        standardButtons: Dialog.Ok
        closePolicy: Popup.CloseOnEscape

        Label {
            id: workspaceErrorLabel

            width: parent.width
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: settingsDialog

        x: Math.round((mainWindow.width - width) / 2)
        y: Math.round((mainWindow.height - height) / 2)
        width: Math.min(mainWindow.width - 80, 620)
        title: qsTr("Settings")
        modal: true
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
