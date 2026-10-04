// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/workspacemanager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>

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
{
}

WorkspaceManager::~WorkspaceManager() = default;

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
    if (!hasActiveWorkspace() || data == m_data)
        return;

    m_data = data;
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
    data.displayName = QFileInfo(path).completeBaseName();

    QString error;
    if (!WorkspaceSerializer::save(path, data, &error)) {
        emit workspaceError(error);
        return false;
    }

    setActiveWorkspace(path, data);
    addOrUpdateRecent(path, data.displayName);
    notify(Diagnostics::Info, [=, this] {
        return tr("Created workspace %1.").arg(data.displayName);
    });
    return true;
}

/*!
 * \brief Opens the workspace at \a pathOrUrl and makes it the active workspace.
 *
 * A file without a stored display name is shown under its file name.
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

    const QString name = result.data.displayName;
    setActiveWorkspace(path, result.data);
    addOrUpdateRecent(path, name);
    notify(Diagnostics::Info, [=, this] {
        return tr("Opened workspace %1.").arg(name);
    });
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
 */
bool WorkspaceManager::writeActiveWorkspaceTo(const QString &path, const QString &displayName)
{
    WorkspaceData data = m_data;
    data.displayName = displayName;

    QString error;
    if (!WorkspaceSerializer::save(path, data, &error)) {
        emit workspaceError(error);
        return false;
    }

    setActiveWorkspace(path, data);
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
 * \brief Makes \a data at \a path the active workspace and clears the dirty flag.
 */
void WorkspaceManager::setActiveWorkspace(const QString &path, const WorkspaceData &data)
{
    m_activePath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    m_data = data;
    setDirty(false);
    emit activeWorkspaceChanged();
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
    m_data = WorkspaceData();
    setDirty(false);
    emit activeWorkspaceChanged();
}

void WorkspaceManager::setDirty(bool dirty)
{
    if (m_dirty == dirty)
        return;
    m_dirty = dirty;
    emit dirtyChanged();
}
