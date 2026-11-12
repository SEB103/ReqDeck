// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/workspacemanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

#include <optional>
#include <utility>

#include "core/workspace/jsonfile.h"
#include "core/workspace/secretstore.h"
#include "core/workspace/workspacebackup.h"
#include "core/workspace/workspaceserializer.h"
#include "framework/apppaths.h"

namespace {
/*!
 * \internal
 * \brief Maximum number of entries kept in the recent-workspaces list.
 */
constexpr int kMaxRecentWorkspaces = 10;

/*!
 * \internal
 * \brief Settings array name holding the recent-workspaces list.
 */
constexpr auto kRecentArrayKey = "RecentWorkspaces";

/*!
 * \internal
 * \brief Settings key holding the default workspaces directory.
 */
constexpr auto kDefaultWorkspacesDirKey = "defaultWorkspacesDir";

/*!
 * \internal
 * \brief Workspace setting that turns cookies off when false.
 */
constexpr QLatin1StringView kCookiesSettingKey("cookies");

/*!
 * \internal
 * \brief Milliseconds between a change of the cookies and writing them.
 */
constexpr int kCookieSaveDelayMs = 500;

/*!
 * \internal
 * \brief Returns a comparable canonical form of \a path for de-duplication.
 *
 * Paths are cleaned and compared case-insensitively so the same workspace file
 * is recognized regardless of separators or drive-letter casing on Windows.
 */
QString canonicalKey(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toLower();
}

/*!
 * \internal
 * \brief Returns \a path with the .reqdeck suffix appended when it is missing.
 */
QString withWorkspaceSuffix(const QString &path)
{
    if (path.endsWith(kWorkspaceFileSuffix, Qt::CaseInsensitive))
        return path;
    return path + kWorkspaceFileSuffix;
}
} // namespace

/*!
    \class WorkspaceManager
    \brief GUI-thread facade that owns the active workspace and the recent-workspaces list.

    \internal
*/

/*!
 * \brief Creates a workspace manager with no active workspace and an empty recent list.
 */
WorkspaceManager::WorkspaceManager(QObject *parent)
    : QObject(parent)
    , m_cookies(new CookieStore(this))
{
    m_cookieSaveTimer.setSingleShot(true);
    m_cookieSaveTimer.setInterval(kCookieSaveDelayMs);
    connect(&m_cookieSaveTimer, &QTimer::timeout, this, &WorkspaceManager::saveCookies);
    connect(m_cookies, &CookieStore::changed, this, [this]() {
        if (!m_cookiesWorkspaceId.isEmpty())
            m_cookieSaveTimer.start();
    });
}

/*!
 * \brief Writes the pending cookies and destroys the workspace manager.
 */
WorkspaceManager::~WorkspaceManager()
{
    saveCookies();
}

/*!
 * \brief Rebuilds the last notification for the status bar after a language switch.
 *
 * Emits statusRetranslated() with the last message re-rendered in the active
 * language. The transient banner is intentionally not re-raised.
 */
void WorkspaceManager::retranslate()
{
    if (m_lastStatus.isValid())
        emit statusRetranslated(m_lastStatus.level, m_lastStatus.render());
}

/*!
 * \internal
 * \brief Emits \a render's text at \a level and stores the renderer for retranslate().
 *
 * The renderer captures its runtime arguments by value, so re-invoking it later
 * re-runs its tr() calls in the active language.
 */
void WorkspaceManager::notify(Diagnostics::Level level, std::function<QString()> render)
{
    m_lastStatus.level = level;
    m_lastStatus.render = std::move(render);
    emit notification(level, m_lastStatus.render());
}

bool WorkspaceManager::hasActiveWorkspace() const
{
    return !m_activePath.isEmpty();
}

QString WorkspaceManager::activeWorkspaceName() const
{
    return m_data.displayName;
}

QString WorkspaceManager::activeWorkspacePath() const
{
    return m_activePath;
}

bool WorkspaceManager::dirty() const
{
    return m_dirty;
}

const WorkspaceData &WorkspaceManager::workspaceData() const
{
    return m_data;
}

/*!
 * \brief Replaces the active workspace's in-memory data with \a data.
 *
 * Marks the workspace dirty when the data actually changed. The data is only
 * written to disk by saveWorkspace() or saveWorkspaceAs(). Ignored when no
 * workspace is active.
 */
void WorkspaceManager::updateWorkspaceData(const WorkspaceData &data)
{
    if (!hasActiveWorkspace())
        return;

    // The id names the secrets file, so a facade must not be able to change it.
    WorkspaceData updated = data;
    updated.id = m_data.id;
    if (updated == m_data)
        return;

    m_data = std::move(updated);
    setDirty(true);
}

/*!
 * \brief Returns the recent-workspace rows exposed to QML, newest first.
 *
 * Each row is a map with \c path, \c displayName, \c lastOpened, and a computed
 * \c available flag that is false when the file is missing.
 */
QVariantList WorkspaceManager::recentWorkspaces() const
{
    QVariantList rows;
    rows.reserve(m_recent.size());
    for (const RecentEntry &entry : m_recent) {
        QVariantMap row;
        row.insert(QStringLiteral("path"), entry.path);
        row.insert(QStringLiteral("displayName"), entry.displayName);
        row.insert(QStringLiteral("lastOpened"), entry.lastOpened);
        row.insert(QStringLiteral("available"), QFileInfo::exists(entry.path));
        rows.append(row);
    }
    return rows;
}

/*!
 * \brief Returns the configured default workspaces directory, or the built-in default.
 *
 * When no directory has been configured, falls back to
 * AppPaths::defaultWorkspacesDir().
 */
QString WorkspaceManager::defaultWorkspacesDir() const
{
    QString dir;
    if (m_settings)
        dir = m_settings->value(QLatin1String(kDefaultWorkspacesDirKey)).toString();

    if (dir.isEmpty())
        dir = AppPaths::instance().defaultWorkspacesDir();
    return QDir::cleanPath(dir);
}

/*!
 * \brief Sets and persists the default workspaces directory from \a pathOrUrl.
 */
void WorkspaceManager::setDefaultWorkspacesDir(const QString &pathOrUrl)
{
    const QString dir = QDir::cleanPath(toLocalPath(pathOrUrl));
    if (dir.isEmpty() || dir == defaultWorkspacesDir())
        return;

    if (m_settings) {
        m_settings->setValue(QLatin1String(kDefaultWorkspacesDirKey), dir);
        m_settings->sync();
    }
    QDir().mkpath(dir);
    emit defaultWorkspacesDirChanged();
}

/*!
 * \brief Sets the directory of the secret values.
 * \param directory The directory, or an empty string for the default.
 */
void WorkspaceManager::setSecretsDir(const QString &directory)
{
    m_secretsDir = directory;
}

/*!
 * \brief Sets the file of the samples workspace.
 * \param path The file, or an empty string for the seeded one.
 */
void WorkspaceManager::setSamplesPath(const QString &path)
{
    m_samplesPath = path;
}

/*!
 * \brief Opens the samples workspace.
 *
 * The samples are an ordinary workspace in the user's data, so changes to them
 * can be saved.
 * \return Whether the workspace was opened.
 */
bool WorkspaceManager::openSamples()
{
    return openWorkspace(m_samplesPath.isEmpty() ? AppPaths::instance().samplesWorkspacePath()
                                                 : m_samplesPath);
}

/*!
 * \brief Returns the configured directory of the secret values, or the default.
 */
QString WorkspaceManager::secretsDir() const
{
    return m_secretsDir.isEmpty() ? AppPaths::instance().secretsDir() : m_secretsDir;
}

/*!
 * \brief Returns the directory of the copies made before a format change.
 *
 * It lies beside the secrets directory, so a test that moves the secrets also
 * moves the copies, and the copies stay in the user's data, never next to a
 * workspace file that may be shared.
 */
QString WorkspaceManager::backupsDir() const
{
    return QDir::cleanPath(QDir(secretsDir()).filePath(QStringLiteral("../backups")));
}

/*!
 * \brief Injects the INI settings store and loads the recent-workspaces list.
 * \param settings Non-owning settings store, or null to disable persistence.
 */
void WorkspaceManager::setSettings(QSettings *settings)
{
    m_settings = settings;
    loadRecentWorkspaces();
}

/*!
 * \brief Creates a new empty workspace named \a name inside \a folderPathOrUrl.
 */
bool WorkspaceManager::createWorkspace(const QString &name, const QString &folderPathOrUrl)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        emit workspaceError(tr("The workspace name must not be empty."));
        return false;
    }

    const QString folder = toLocalPath(folderPathOrUrl);
    if (folder.isEmpty()) {
        emit workspaceError(tr("A workspace folder must be selected."));
        return false;
    }

    return createWorkspaceAtPath(QDir(folder).filePath(withWorkspaceSuffix(trimmedName)));
}

/*!
 * \brief Creates a new empty workspace at the .reqdeck path \a pathOrUrl and opens it.
 *
 * An existing file is never overwritten; creating over it fails with
 * workspaceError().
 */
bool WorkspaceManager::createWorkspaceAtPath(const QString &pathOrUrl)
{
    const QString localPath = toLocalPath(pathOrUrl);
    if (localPath.isEmpty()) {
        emit workspaceError(tr("A workspace location must be selected."));
        return false;
    }
    const QString path = withWorkspaceSuffix(localPath);

    if (QFileInfo::exists(path)) {
        emit workspaceError(tr("A workspace file already exists at %1.").arg(path));
        return false;
    }

    WorkspaceData data;
    data.id = newItemId();
    data.displayName = QFileInfo(path).completeBaseName();

    QString error;
    if (!WorkspaceSerializer::save(path, data, &error)) {
        emit workspaceError(error);
        return false;
    }

    setActiveWorkspace(path, data);
    emit workspaceReplaced();
    addOrUpdateRecent(path, data.displayName);
    notify(Diagnostics::Info, [=, this] {
        return tr("Created workspace %1.").arg(data.displayName);
    });
    return true;
}

/*!
 * \brief Opens the workspace at \a pathOrUrl and makes it the active workspace.
 *
 * A file without a stored display name is shown under its file name. A file
 * without an id gets one that is written with the next save, which does not
 * count as an unsaved change. Secret values that cannot be read are reported as
 * a warning, because the workspace itself is intact. A file that could not be
 * read at all stays protected: the next save reads it again first. A plain
 * secrets file of an older version is copied (encrypted) and then encrypted at
 * once. One encrypted on another computer or by another user is reported and
 * kept; saving is allowed, and the file is copied and replaced only once a
 * secret value is saved here.
 */
bool WorkspaceManager::openWorkspace(const QString &pathOrUrl)
{
    const QString path = toLocalPath(pathOrUrl);
    WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    if (!result.ok) {
        emit workspaceError(result.errorString);
        return false;
    }

    if (result.data.displayName.isEmpty())
        result.data.displayName = QFileInfo(path).completeBaseName();

    if (result.data.id.isEmpty())
        result.data.id = newItemId();

    const QString name = result.data.displayName;
    const SecretStore store(secretsDir());
    const SecretStore::ReadStatus secrets = store.applyTo(result.data);
    QString migrationError;
    bool migrationFailed = false;
    bool copyFailed = false;
    if (secrets == SecretStore::ReadStatus::Ok && store.isPlain(result.data.id)) {
        // The copy is made first and holds the old values encrypted; encrypting
        // the file still goes ahead without it, because plain values are worse.
        copyFailed = !keepPreviousCopy(path, result.data.id);
        migrationFailed = !store.save(result.data, &migrationError);
    }

    setActiveWorkspace(path, result.data);
    // Only what the file holds counts as stored: a value kept in the workspace
    // file must reach the secrets file with the next save, changed or not.
    m_storedSecrets = store.storedValues(result.data);
    m_secretsUnavailable = secrets == SecretStore::ReadStatus::Unavailable;
    emit workspaceReplaced();
    setSecretsUnread(secrets == SecretStore::ReadStatus::Unreadable);
    addOrUpdateRecent(path, name);
    notify(Diagnostics::Info, [=, this] {
        return tr("Opened workspace %1.").arg(name);
    });
    if (secrets == SecretStore::ReadStatus::Unavailable) {
        notify(Diagnostics::Warning, [=, this] {
            return tr("The secret values of workspace %1 were saved on another computer or by another "
                      "Windows user and cannot be read here. Enter them again.")
                .arg(name);
        });
    } else if (secrets != SecretStore::ReadStatus::Ok) {
        notify(Diagnostics::Warning, [=, this] {
            return tr("The secret values of workspace %1 could not be read.").arg(name);
        });
    } else if (migrationFailed) {
        notify(Diagnostics::Warning, [=, this] {
            return tr("The secret values of workspace %1 are still stored unencrypted: %2")
                .arg(name, migrationError);
        });
    } else if (copyFailed) {
        notify(Diagnostics::Warning, [=, this] {
            return tr("The secret values of workspace %1 were encrypted, but no copy of the old file "
                      "could be made.")
                .arg(name);
        });
    }
    return true;
}

/*!
 * \brief Opens the recent-workspace entry at \a index.
 */
bool WorkspaceManager::openRecent(int index)
{
    if (index < 0 || index >= m_recent.size())
        return false;
    return openWorkspace(m_recent.at(index).path);
}

/*!
 * \brief Saves the active workspace to its current file.
 */
bool WorkspaceManager::saveWorkspace()
{
    if (!hasActiveWorkspace()) {
        emit workspaceError(tr("There is no active workspace to save."));
        return false;
    }
    return writeActiveWorkspaceTo(m_activePath, m_data.displayName);
}

/*!
 * \brief Saves the active workspace to \a pathOrUrl, which becomes the active file.
 */
bool WorkspaceManager::saveWorkspaceAs(const QString &pathOrUrl)
{
    if (!hasActiveWorkspace()) {
        emit workspaceError(tr("There is no active workspace to save."));
        return false;
    }

    const QString localPath = toLocalPath(pathOrUrl);
    if (localPath.isEmpty()) {
        emit workspaceError(tr("A workspace location must be selected."));
        return false;
    }
    const QString path = withWorkspaceSuffix(localPath);

    return writeActiveWorkspaceTo(path, QFileInfo(path).completeBaseName());
}

/*!
 * \brief Writes the active workspace data to \a path under \a displayName.
 *
 * On success the file becomes the active workspace under \a displayName and the
 * dirty flag is cleared; on failure workspaceError() is emitted and the active
 * workspace is left unchanged.
 *
 * A write to another file creates a separate workspace, which gets its own id
 * and therefore its own secrets; the original keeps its secrets file.
 *
 * Secrets that could not be read on open are read again first, so values the
 * user never saw are not replaced by empty ones; while the file stays unreadable
 * saving is refused. The secret values are then written before the workspace
 * file, and a failure of the latter puts the previous secrets back: both files
 * change together or not at all. Unchanged secret values of the same workspace
 * are not written again, because encryption gives a different file each time.
 * A file of an older format is copied first (see keepPreviousCopy()); without
 * the copy it is not rewritten.
 */
bool WorkspaceManager::writeActiveWorkspaceTo(const QString &path, const QString &displayName)
{
    WorkspaceData data = m_data;
    data.displayName = displayName;

    const SecretStore store(secretsDir());
    if (m_secretsUnread
        && store.fillEmptyValues(data) == SecretStore::ReadStatus::Unreadable) {
        emit workspaceError(tr("The secret values of workspace %1 could not be read, "
                               "so it was not saved. Try again.")
                                .arg(displayName));
        return false;
    }

    const bool sameFile = canonicalKey(path) == canonicalKey(m_activePath);
    // A plain file left by a failed encryption on open is written again in any case.
    const bool writeSecrets =
        !sameFile || SecretStore::valuesOf(data) != m_storedSecrets || store.isPlain(data.id);
    const bool replacesForeign = sameFile && writeSecrets && m_secretsUnavailable;
    if (sameFile && (data.schemaVersion < kWorkspaceSchemaVersion || replacesForeign)
        && !keepPreviousCopy(m_activePath, data.id)) {
        emit workspaceError(tr("A copy of workspace %1 could not be made, so it was not saved.")
                                .arg(displayName));
        return false;
    }
    data.schemaVersion = kWorkspaceSchemaVersion;
    if (!sameFile)
        data.id = newItemId();

    std::optional<QByteArray> previousSecrets;
    QString error;
    if (writeSecrets) {
        previousSecrets = store.snapshot(data.id);
        if (!previousSecrets && QFile::exists(store.filePath(data.id))) {
            emit workspaceError(tr("The secret values of workspace %1 could not be read, "
                                   "so it was not saved. Try again.")
                                    .arg(displayName));
            return false;
        }
        if (!store.save(data, &error)) {
            emit workspaceError(error);
            return false;
        }
    }
    if (!WorkspaceSerializer::save(path, data, &error)) {
        if (writeSecrets)
            store.restore(data.id, previousSecrets);
        emit workspaceError(error);
        return false;
    }

    const bool stillUnavailable = m_secretsUnavailable && sameFile && !writeSecrets;
    setActiveWorkspace(path, data);
    m_secretsUnavailable = stillUnavailable;
    addOrUpdateRecent(path, displayName);
    notify(Diagnostics::Info, [=, this] {
        return tr("Saved workspace %1.").arg(displayName);
    });
    return true;
}

/*!
 * \brief Closes the active workspace and discards its in-memory data.
 *
 * Unsaved changes are discarded; the caller asks the user first.
 */
void WorkspaceManager::closeWorkspace()
{
    const QString closedName = m_data.displayName;
    const bool wasActive = hasActiveWorkspace();

    clearActiveWorkspace();

    if (wasActive)
        notify(Diagnostics::Info, [=, this] {
            return tr("Closed workspace %1.").arg(closedName);
        });
}

/*!
 * \brief Removes the recent-workspace entry at \a index from the list and the store.
 */
void WorkspaceManager::removeRecent(int index)
{
    if (index < 0 || index >= m_recent.size())
        return;

    m_recent.removeAt(index);
    saveRecentWorkspaces();
    emit recentWorkspacesChanged();
}

/*!
 * \brief Returns the local file path for \a pathOrUrl, converting file: URLs.
 *
 * QML file dialogs deliver file: URLs while stored recent entries and command
 * lines use plain paths; this normalizes both to a local filesystem path.
 */
QString WorkspaceManager::toLocalPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    if (url.isLocalFile())
        return url.toLocalFile();
    return pathOrUrl;
}

/*!
 * \brief Loads the recent-workspaces list from the injected settings store.
 */
void WorkspaceManager::loadRecentWorkspaces()
{
    m_recent.clear();

    if (m_settings) {
        const int count = m_settings->beginReadArray(QLatin1String(kRecentArrayKey));
        for (int i = 0; i < count; ++i) {
            m_settings->setArrayIndex(i);
            RecentEntry entry;
            entry.path = m_settings->value(QStringLiteral("path")).toString();
            entry.displayName = m_settings->value(QStringLiteral("displayName")).toString();
            entry.lastOpened = m_settings->value(QStringLiteral("lastOpened")).toString();
            if (!entry.path.isEmpty())
                m_recent.append(entry);
        }
        m_settings->endArray();
    }

    emit recentWorkspacesChanged();
}

/*!
 * \brief Writes the recent-workspaces list to the injected settings store.
 */
void WorkspaceManager::saveRecentWorkspaces()
{
    if (!m_settings)
        return;

    m_settings->beginWriteArray(QLatin1String(kRecentArrayKey), int(m_recent.size()));
    for (int i = 0; i < m_recent.size(); ++i) {
        m_settings->setArrayIndex(i);
        const RecentEntry &entry = m_recent.at(i);
        m_settings->setValue(QStringLiteral("path"), entry.path);
        m_settings->setValue(QStringLiteral("displayName"), entry.displayName);
        m_settings->setValue(QStringLiteral("lastOpened"), entry.lastOpened);
    }
    m_settings->endArray();
    m_settings->sync();
}

/*!
 * \brief Inserts or refreshes the recent entry for \a path at the front of the list.
 *
 * An existing entry for the same file (compared case-insensitively) is removed
 * first, so the workspace moves to the front with a fresh timestamp and
 * \a displayName instead of being duplicated. The list is capped at
 * kMaxRecentWorkspaces entries.
 */
void WorkspaceManager::addOrUpdateRecent(const QString &path, const QString &displayName)
{
    const QString absolutePath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString key = canonicalKey(absolutePath);

    for (qsizetype i = m_recent.size() - 1; i >= 0; --i) {
        if (canonicalKey(m_recent.at(i).path) == key)
            m_recent.removeAt(i);
    }

    RecentEntry entry;
    entry.path = absolutePath;
    entry.displayName = displayName;
    entry.lastOpened = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_recent.prepend(entry);

    while (m_recent.size() > kMaxRecentWorkspaces)
        m_recent.removeLast();

    saveRecentWorkspaces();
    emit recentWorkspacesChanged();
}

/*!
 * \brief Returns the cookies of the active workspace, owned by the manager.
 *
 * The store always exists; without an active workspace it is empty.
 */
CookieStore *WorkspaceManager::cookieStore() const
{
    return m_cookies;
}

/*!
 * \brief Returns whether requests of the active workspace send and store cookies.
 *
 * Stored in the workspace settings as "cookies": false when turned off.
 */
bool WorkspaceManager::cookiesEnabled() const
{
    return m_data.settings.value(kCookiesSettingKey).toBool(true);
}

/*!
 * \brief Sets whether requests of the active workspace send and store cookies.
 *
 * The stored cookies stay when cookies are turned off, so turning them on
 * again picks up the session. Ignored without an active workspace.
 * \param enabled True to send and store cookies.
 */
void WorkspaceManager::setCookiesEnabled(bool enabled)
{
    if (!hasActiveWorkspace() || enabled == cookiesEnabled())
        return;
    WorkspaceData data = m_data;
    if (enabled)
        data.settings.remove(kCookiesSettingKey);
    else
        data.settings.insert(kCookiesSettingKey, false);
    updateWorkspaceData(data);
    emit cookiesEnabledChanged();
}

/*!
 * \brief Writes the persistent cookies of the active workspace now.
 *
 * Normally they are written shortly after they change, apart from saving the
 * workspace, since responses set them. A failure goes to the log; the cookies
 * stay in memory.
 */
void WorkspaceManager::saveCookies()
{
    m_cookieSaveTimer.stop();
    if (m_cookiesWorkspaceId.isEmpty() || m_cookiesUnwritable)
        return;
    QString error;
    if (!m_cookies->save(cookiesFilePath(m_cookiesWorkspaceId), SecretCipher(), &error))
        notify(Diagnostics::Level::Warning, [error]() { return error; });
}

/*!
 * \internal
 * \brief Switches the cookie store to the workspace \a workspaceId.
 *
 * The cookies of the previous workspace are written first. Saving a
 * workspace under another name keeps its id and therefore its cookies.
 * \param workspaceId The id of the workspace now active, or empty for none.
 */
void WorkspaceManager::switchCookies(const QString &workspaceId)
{
    if (workspaceId == m_cookiesWorkspaceId)
        return;
    saveCookies();
    m_cookiesWorkspaceId.clear();
    if (!isValidWorkspaceId(workspaceId)) {
        m_cookies->clear();
        return;
    }
    QString error;
    m_cookiesUnwritable = !m_cookies->load(cookiesFilePath(workspaceId), SecretCipher(), &error);
    if (m_cookiesUnwritable) {
        // Like secrets of another computer: the file stays as it is, and the
        // cookies of this session live until the workspace closes.
        notify(Diagnostics::Level::Warning, [error]() {
            return QCoreApplication::translate("WorkspaceManager",
                                               "%1 The file is left unchanged; new cookies are kept until the workspace closes.")
                .arg(error);
        });
    }
    m_cookiesWorkspaceId = workspaceId;
}

/*!
 * \internal
 * \brief Returns the cookies file of \a workspaceId beside its secret values.
 */
QString WorkspaceManager::cookiesFilePath(const QString &workspaceId) const
{
    return QDir(secretsDir()).filePath(workspaceId + QStringLiteral(".cookies.json"));
}

/*!
 * \brief Makes \a data at \a path the active workspace and clears the dirty flag.
 */
void WorkspaceManager::setActiveWorkspace(const QString &path, const WorkspaceData &data)
{
    setSecretsUnread(false);
    m_activePath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const bool cookiesWereEnabled = cookiesEnabled();
    m_data = data;
    switchCookies(data.id);
    if (cookiesEnabled() != cookiesWereEnabled)
        emit cookiesEnabledChanged();
    m_storedSecrets = SecretStore::valuesOf(data);
    m_secretsUnavailable = false;
    setDirty(false);
    emit activeWorkspaceChanged();
    emit previousCopyChanged();
}

/*!
 * \brief Clears the active workspace, its data, and the dirty flag.
 */
void WorkspaceManager::clearActiveWorkspace()
{
    if (m_activePath.isEmpty()) {
        m_data = WorkspaceData();
        setDirty(false);
        return;
    }
    m_activePath.clear();
    const bool cookiesWereEnabled = cookiesEnabled();
    m_data = WorkspaceData();
    m_storedSecrets = {};
    switchCookies(QString());
    if (cookiesEnabled() != cookiesWereEnabled)
        emit cookiesEnabledChanged();
    setDirty(false);
    emit activeWorkspaceChanged();
    emit previousCopyChanged();
    emit workspaceReplaced();
}

/*!
 * \internal
 * \brief Copies the workspace file at \a path and the secrets of \a workspaceId.
 *
 * Called before a file format changes and before secrets encrypted elsewhere
 * are replaced. A copy that exists is replaced, so the copy always shows the
 * files as they were before the latest such change. Plain secrets are copied
 * encrypted (see SecretStore::sealedSnapshot()).
 * \param path The workspace file.
 * \param workspaceId The UUID of the workspace.
 * \return True when the copy was written.
 */
bool WorkspaceManager::keepPreviousCopy(const QString &path, const QString &workspaceId)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    WorkspaceBackup::Copy copy;
    copy.workspace = file.readAll();
    if (!SecretStore(secretsDir()).sealedSnapshot(workspaceId, copy.secrets)
        || !WorkspaceBackup(backupsDir()).save(workspaceId, copy)) {
        return false;
    }
    emit previousCopyChanged();
    return true;
}

bool WorkspaceManager::hasPreviousCopy() const
{
    return hasActiveWorkspace() && WorkspaceBackup(backupsDir()).exists(m_data.id);
}

QString WorkspaceManager::previousCopyDate() const
{
    if (!hasActiveWorkspace())
        return {};
    const QDateTime created = WorkspaceBackup(backupsDir()).created(m_data.id);
    return created.isValid() ? QLocale().toString(created, QLocale::ShortFormat) : QString();
}

/*!
 * \brief Puts back the copy made before the last format change.
 *
 * The secrets go first and the workspace file second; when the second write
 * fails, the secrets are put back as they were. The workspace then opens from
 * the restored files, so unsaved changes are lost; the caller asks first. The
 * copy stays, so the restore can be repeated.
 * \return True when the files were restored and the workspace opened again.
 */
bool WorkspaceManager::restorePreviousCopy()
{
    const std::optional<WorkspaceBackup::Copy> copy =
        hasActiveWorkspace() ? WorkspaceBackup(backupsDir()).load(m_data.id) : std::nullopt;
    if (!copy) {
        emit workspaceError(tr("There is no previous copy of this workspace."));
        return false;
    }

    const QString path = m_activePath;
    const SecretStore store(secretsDir());
    const std::optional<QByteArray> currentSecrets = store.snapshot(m_data.id);
    // Without the current secrets there would be nothing to put back if the
    // workspace file cannot be written.
    if ((!currentSecrets && QFile::exists(store.filePath(m_data.id))) || !store.restore(m_data.id, copy->secrets)) {
        emit workspaceError(tr("The previous copy could not be restored."));
        return false;
    }
    if (!writeFileAtomically(path, copy->workspace)) {
        store.restore(m_data.id, currentSecrets);
        emit workspaceError(tr("The previous copy could not be restored."));
        return false;
    }
    if (!openWorkspace(path))
        return false;
    notify(Diagnostics::Info, [=, this] {
        return tr("Restored the previous copy of workspace %1.").arg(m_data.displayName);
    });
    return true;
}

/*!
 * \brief Returns whether the secret values are still to be read.
 */
bool WorkspaceManager::secretsUnread() const
{
    return m_secretsUnread;
}

/*!
 * \brief Reads secret values that could not be read before.
 *
 * The values match the stored file, so this is no unsaved change. Rows are
 * matched by key and position, which is why variables must not change until the
 * values are read.
 * \return Whether none are left to read.
 */
bool WorkspaceManager::retrySecrets()
{
    if (!m_secretsUnread)
        return true;
    if (SecretStore(secretsDir()).fillEmptyValues(m_data) == SecretStore::ReadStatus::Unreadable)
        return false;
    setSecretsUnread(false);
    return true;
}

/*!
 * \internal
 * \brief Records whether the secret values are still to be read.
 * \param unread True when they could not be read.
 */
void WorkspaceManager::setSecretsUnread(bool unread)
{
    if (m_secretsUnread == unread)
        return;
    m_secretsUnread = unread;
    emit secretsUnreadChanged();
}

void WorkspaceManager::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged();
}
