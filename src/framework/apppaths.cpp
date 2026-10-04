// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "framework/apppaths.h"

#include "productinfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
/*!
 * \internal
 * \brief Marker file that selects portable mode when present next to the exe.
 */
constexpr auto kPortableMarker = "portable.ini";
} // namespace

/*!
 * \brief Returns the process-wide AppPaths instance.
 */
AppPaths& AppPaths::instance()
{
    static AppPaths paths;
    return paths;
}

/*!
 * \brief Resolves the run mode and all base directories.
 *
 * Portable mode is selected when a \c portable.ini marker sits next to the
 * executable. In portable mode every writable directory lives under the
 * application directory; in installed mode settings go to AppConfigLocation,
 * logs and other application data to AppLocalDataLocation, and workspaces
 * default to \c Documents/<product identifier>.
 *
 * The organization and application names must already be set on
 * QCoreApplication, because the installed-mode locations derive from them.
 * Calling this more than once is a no-op.
 */
void AppPaths::initialize()
{
    if (m_initialized)
        return;

    m_seedDir = QCoreApplication::applicationDirPath();
    m_portable = QFileInfo::exists(
        QDir(m_seedDir).filePath(QString::fromLatin1(kPortableMarker)));

    if (m_portable) {
        const QDir root(m_seedDir);
        m_configDir = root.filePath(QStringLiteral("config"));
        m_dataDir = root.filePath(QStringLiteral("data"));
        m_localDataDir = m_dataDir;
        m_logDir = root.filePath(QStringLiteral("logs"));
        m_defaultWorkspacesDir = QDir(m_dataDir).filePath(QStringLiteral("workspaces"));
    } else {
        m_configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        m_localDataDir = m_dataDir;
        m_logDir = QDir(m_localDataDir).filePath(QStringLiteral("log"));
        const QString documents =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        // Use the stable product identifier (not the running executable name) so
        // the workspaces folder is consistent across the app and its tests.
        m_defaultWorkspacesDir =
            QDir(documents).filePath(QStringLiteral(PRODUCT_IDENTIFIER));
    }

    m_seedDir = QDir::cleanPath(m_seedDir);
    m_configDir = QDir::cleanPath(m_configDir);
    m_dataDir = QDir::cleanPath(m_dataDir);
    m_localDataDir = QDir::cleanPath(m_localDataDir);
    m_logDir = QDir::cleanPath(m_logDir);
    m_defaultWorkspacesDir = QDir::cleanPath(m_defaultWorkspacesDir);

    m_initialized = true;
}

/*!
 * \internal
 * \brief Resolves paths on first access when initialize() has not been called.
 *
 * Production code calls initialize() explicitly from main(); this guard keeps
 * the getters correct for callers (such as tests) that construct dependent
 * objects without that call.
 */
void AppPaths::ensureInitialized() const
{
    if (!m_initialized)
        const_cast<AppPaths *>(this)->initialize();
}

/*!
 * \brief Returns whether the application runs in portable mode.
 */
bool AppPaths::isPortable() const
{
    ensureInitialized();
    return m_portable;
}

QString AppPaths::seedDir() const
{
    ensureInitialized();
    return m_seedDir;
}

QString AppPaths::configDir() const
{
    ensureInitialized();
    return m_configDir;
}

QString AppPaths::dataDir() const
{
    ensureInitialized();
    return m_dataDir;
}

QString AppPaths::localDataDir() const
{
    ensureInitialized();
    return m_localDataDir;
}

QString AppPaths::logDir() const
{
    ensureInitialized();
    return m_logDir;
}

QString AppPaths::defaultWorkspacesDir() const
{
    ensureInitialized();
    return m_defaultWorkspacesDir;
}
