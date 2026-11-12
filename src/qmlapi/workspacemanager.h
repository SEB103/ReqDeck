// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKSPACEMANAGER_H
#define WORKSPACEMANAGER_H

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <functional>

#include "core/http/cookiestore.h"
#include "core/workspace/workspacedata.h"
#include "framework/diagnosticslevel.h"
#include "framework/retranslatablestatus.h"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

/**
 * GUI-thread facade that owns the active workspace and the recent-workspaces list.
 *
 * Exposed to QML as the \c cppWorkspaceManager context property. The active
 * workspace's .reqdeck file is the single source of truth for workspace data;
 * this class holds that data in memory while the workspace is open and writes it
 * back on save. Values of secret variables stay out of that file and are joined
 * from a SecretStore on open and written there on save. The recent-workspaces
 * list is application-global metadata and is stored in the injected QSettings
 * store, never inside a workspace file.
 */
class WorkspaceManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WorkspaceManager)

    /** Whether a workspace is currently active. */
    Q_PROPERTY(bool hasActiveWorkspace READ hasActiveWorkspace NOTIFY activeWorkspaceChanged)

    /** Display name of the active workspace, or an empty string when none is active. */
    Q_PROPERTY(QString activeWorkspaceName READ activeWorkspaceName NOTIFY activeWorkspaceChanged)

    /** Absolute file path of the active workspace's .reqdeck file, or an empty string. */
    Q_PROPERTY(QString activeWorkspacePath READ activeWorkspacePath NOTIFY activeWorkspaceChanged)

    /** Whether the active workspace has unsaved changes. */
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)

    /** Whether the secret values could not be read when the workspace was opened. */
    Q_PROPERTY(bool secretsUnread READ secretsUnread NOTIFY secretsUnreadChanged)

    /** Whether a copy of the active workspace from before a format change exists. */
    Q_PROPERTY(bool hasPreviousCopy READ hasPreviousCopy NOTIFY previousCopyChanged)

    /** When that copy was made, as local date and time text; empty without one. */
    Q_PROPERTY(QString previousCopyDate READ previousCopyDate NOTIFY previousCopyChanged)

    /** Recent-workspace entries, newest first, as QVariantMap rows for QML. */
    Q_PROPERTY(QVariantList recentWorkspaces READ recentWorkspaces NOTIFY recentWorkspacesChanged)

    /** Default directory new workspaces are created in; user-configurable. */
    Q_PROPERTY(QString defaultWorkspacesDir READ defaultWorkspacesDir WRITE setDefaultWorkspacesDir
                   NOTIFY defaultWorkspacesDirChanged)

    /** Whether requests of the active workspace send and store cookies; a workspace setting. */
    Q_PROPERTY(bool cookiesEnabled READ cookiesEnabled WRITE setCookiesEnabled NOTIFY cookiesEnabledChanged)

public:
    /** Creates a workspace manager with no active workspace and an empty recent list. */
    explicit WorkspaceManager(QObject *parent = nullptr);

    /** Destroys the workspace manager. */
    ~WorkspaceManager() override;

    /** Returns whether a workspace is currently active. */
    bool hasActiveWorkspace() const;

    /** Returns the display name of the active workspace, or an empty string. */
    QString activeWorkspaceName() const;

    /** Returns the absolute .reqdeck path of the active workspace, or an empty string. */
    QString activeWorkspacePath() const;

    /** Returns whether the active workspace has unsaved changes. */
    bool dirty() const;

    /** Returns whether the secret values are still to be read. */
    bool secretsUnread() const;

    /** Reads secret values that could not be read before; returns whether none are left. */
    Q_INVOKABLE bool retrySecrets();

    /** Returns whether a copy of the active workspace from before a format change exists. */
    bool hasPreviousCopy() const;

    /** Returns when that copy was made, or an empty string. */
    QString previousCopyDate() const;

    /**
     * Puts back the copied workspace file and secrets and opens the workspace again;
     * unsaved changes are lost. Returns false and emits workspaceError() without a copy.
     */
    Q_INVOKABLE bool restorePreviousCopy();

    /** Returns the recent-workspace rows exposed to QML, newest first. */
    QVariantList recentWorkspaces() const;

    /** Returns the configured default workspaces directory, or the built-in default. */
    QString defaultWorkspacesDir() const;

    /** Sets and persists the default workspaces directory from \a pathOrUrl. */
    Q_INVOKABLE void setDefaultWorkspacesDir(const QString &pathOrUrl);

    /**
     * Injects the INI settings store used to persist the recent-workspaces list.
     * Ownership stays with the caller; passing null disables persistence.
     */
    void setSettings(QSettings *settings);

    /** Sets the directory of the secret values; an empty string restores the default. */
    void setSecretsDir(const QString &directory);

    /** Sets the file of the samples workspace; an empty path means the seeded one. */
    void setSamplesPath(const QString &path);

    /** Opens the samples workspace; failures are reported like those of openWorkspace(). */
    Q_INVOKABLE bool openSamples();

    /** Returns the configured directory of the secret values, or AppPaths::secretsDir(). */
    QString secretsDir() const;

    /** Returns the directory of the copies: "backups" beside the secrets directory. */
    QString backupsDir() const;

    /** Returns the cookies of the active workspace; empty without one; owned by the manager. */
    CookieStore *cookieStore() const;

    /** Returns whether requests of the active workspace send and store cookies; true by default. */
    bool cookiesEnabled() const;

    /** Sets whether requests of the active workspace send and store cookies; marks the workspace dirty. */
    void setCookiesEnabled(bool enabled);

    /** Writes the cookies of the active workspace now instead of after the short delay. */
    void saveCookies();

    /** Returns the in-memory data of the active workspace (empty when none is active). */
    const WorkspaceData &workspaceData() const;

    /**
     * Replaces the in-memory data of the active workspace with \a data and marks
     * the workspace dirty when it changed. Ignored when no workspace is active.
     */
    void updateWorkspaceData(const WorkspaceData &data);

    /**
     * Creates a new empty workspace named \a name inside \a folderPathOrUrl and
     * opens it. Returns true on success; on failure emits workspaceError() and
     * returns false.
     */
    Q_INVOKABLE bool createWorkspace(const QString &name, const QString &folderPathOrUrl);

    /**
     * Creates a new empty workspace at the .reqdeck path \a pathOrUrl (as returned
     * by a save file dialog), deriving the display name from the file name, and
     * opens it. Returns true on success; on failure emits workspaceError() and
     * returns false.
     */
    Q_INVOKABLE bool createWorkspaceAtPath(const QString &pathOrUrl);

    /**
     * Opens the workspace at \a pathOrUrl: validates and loads the file and makes
     * it the active workspace. Returns true on success; on failure emits
     * workspaceError() and returns false.
     */
    Q_INVOKABLE bool openWorkspace(const QString &pathOrUrl);

    /** Opens the recent-workspace entry at \a index. Returns false when out of range. */
    Q_INVOKABLE bool openRecent(int index);

    /**
     * Saves the active workspace to its current file, clearing the dirty flag.
     * Returns true on success; on failure emits workspaceError() and returns false.
     */
    Q_INVOKABLE bool saveWorkspace();

    /**
     * Saves the active workspace to \a pathOrUrl, which becomes the active
     * workspace file, and derives the display name from the file name. Returns
     * true on success; on failure emits workspaceError() and returns false.
     */
    Q_INVOKABLE bool saveWorkspaceAs(const QString &pathOrUrl);

    /** Closes the active workspace and discards its in-memory data. */
    Q_INVOKABLE void closeWorkspace();

    /** Removes the recent-workspace entry at \a index from the list and the store. */
    Q_INVOKABLE void removeRecent(int index);

    /** Returns the local file path for \a pathOrUrl, converting file: URLs. */
    Q_INVOKABLE static QString toLocalPath(const QString &pathOrUrl);

    /**
     * Rebuilds the last notification's text in the active language after a UI
     * language switch, so the persistent status bar re-translates live. Emits
     * statusRetranslated() without re-raising the transient notification banner.
     */
    void retranslate();

signals:
    /** Emitted when the active workspace, its name, or its path changes. */
    void activeWorkspaceChanged();

    /** Emitted when another workspace was opened or created, or the active one closed. */
    void workspaceReplaced();

    /** Emitted when the unsaved-changes state changes. */
    void dirtyChanged();

    /** Emitted when the secret values were read or could not be read. */
    void secretsUnreadChanged();

    /** Emitted when a copy was made, restored, or another workspace became active. */
    void previousCopyChanged();

    /** Emitted when the recent-workspaces list changes. */
    void recentWorkspacesChanged();

    /** Emitted when the default workspaces directory changes. */
    void defaultWorkspacesDirChanged();

    /** Emitted when cookiesEnabled() changes. */
    void cookiesEnabledChanged();

    /** Emitted with a user-facing \a message when a workspace operation fails. */
    void workspaceError(const QString &message);

    /**
     * Reports a user-facing outcome of a workspace operation.
     *
     * \a level is a Diagnostics::Level value and \a message is ready to be
     * shown as-is. Failures are reported through workspaceError(), which opens
     * a modal dialog for the ones the user must act on.
     */
    void notification(int level, const QString &message);

    /**
     * Re-emits the last notification's text rebuilt in the active language for
     * the persistent status bar only; emitted from retranslate() and must not
     * re-raise the transient notification banner.
     */
    void statusRetranslated(int level, const QString &message);

protected:
    /**
     * Emits a notification built by \a render at severity \a level and stores the
     * renderer so retranslate() can rebuild the status bar text after a live UI
     * language switch.
     */
    void notify(Diagnostics::Level level, std::function<QString()> render);

    /** Loads the recent-workspaces list from the injected settings store. */
    void loadRecentWorkspaces();

    /** Writes the recent-workspaces list to the injected settings store. */
    void saveRecentWorkspaces();

    /**
     * Inserts or refreshes the recent entry for \a path, moving it to the front
     * with the current timestamp and \a displayName. Caps the list.
     */
    void addOrUpdateRecent(const QString &path, const QString &displayName);

    /** Writes the active workspace data to \a path as \a displayName. */
    bool writeActiveWorkspaceTo(const QString &path, const QString &displayName);

    /** Makes \a data at \a path the active workspace and clears the dirty flag. */
    void setActiveWorkspace(const QString &path, const WorkspaceData &data);

    /** Clears the active workspace, its data, and the dirty flag. */
    void clearActiveWorkspace();

    /** Sets the unsaved-changes flag to \a dirty and emits on change. */
    void setDirty(bool dirty);

    /** Records whether the secret values are still to be read. */
    void setSecretsUnread(bool unread);

    /** One recent-workspace entry as stored in settings. */
    struct RecentEntry
    {
        /** Absolute .reqdeck file path. */
        QString path;
        /** Display name last seen for the workspace. */
        QString displayName;
        /** Last-opened timestamp in ISO 8601 format. */
        QString lastOpened;
    };

    /** Non-owning INI settings store for the recent-workspaces list. */
    QSettings *m_settings {nullptr};

    /** Recent-workspace entries in display order, newest first. */
    QList<RecentEntry> m_recent;

    /** Configured directory of the secret values; empty selects the default. */
    QString m_secretsDir;

    /** Configured file of the samples workspace; empty means the seeded one. */
    QString m_samplesPath;

    /** Cookies of the active workspace; owned through QObject. */
    CookieStore *m_cookies {nullptr};

    /** Id of the workspace whose cookies m_cookies holds; empty for none. */
    QString m_cookiesWorkspaceId;

    /** Whether the cookies file of the workspace could not be read, so it is never written. */
    bool m_cookiesUnwritable {false};

    /** Delays writing the cookies so a burst of responses writes once. */
    QTimer m_cookieSaveTimer;

    /** Switches m_cookies to the workspace \a workspaceId, saving those of the previous one. */
    void switchCookies(const QString &workspaceId);

    /** Returns the cookies file of \a workspaceId beside its secret values. */
    QString cookiesFilePath(const QString &workspaceId) const;

    /** Absolute .reqdeck path of the active workspace; empty when none is active. */
    QString m_activePath;

    /** In-memory data of the active workspace; default-constructed when none is active. */
    WorkspaceData m_data;

    /** Whether the active workspace has unsaved changes. */
    bool m_dirty {false};

    /** Copies \a path and the secrets of \a workspaceId before they change for good. */
    bool keepPreviousCopy(const QString &path, const QString &workspaceId);

    /** Whether stored secrets of the active workspace could not be read yet. */
    bool m_secretsUnread {false};

    /** Secret values as the secrets file holds them, so that an unchanged save does not rewrite it. */
    QJsonObject m_storedSecrets;

    /** Whether the secrets file was encrypted elsewhere and is not copied yet. */
    bool m_secretsUnavailable {false};

    /** Last notification, kept re-buildable so retranslate() can refresh it. */
    RetranslatableStatus m_lastStatus;
};

#endif // WORKSPACEMANAGER_H
