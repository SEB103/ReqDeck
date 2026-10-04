// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APPPATHS_H
#define APPPATHS_H

#include <QString>

/**
 * Central policy for where the application reads and writes its files.
 *
 * The application runs in one of two modes, chosen once at startup:
 * - Portable mode, when a portable.ini marker sits next to the executable: all
 *   user data lives under the application directory (config/, data/, logs/), so
 *   the whole installation can be moved together with its data.
 * - Installed mode otherwise: read-only binaries live in the install directory
 *   while user data lives in the per-user locations (AppConfigLocation for
 *   settings, AppLocalDataLocation for logs and other application data) and
 *   workspaces default to the user's Documents folder.
 *
 * All portable/installed decisions are made here so that no other code needs to
 * test the mode. Call initialize() once after QCoreApplication is constructed
 * and its organization/application names are set; every getter is valid
 * afterwards.
 */
class AppPaths
{
public:
    /** Returns the process-wide instance. */
    static AppPaths& instance();

    /** Resolves the run mode and base directories; safe to call more than once. */
    void initialize();

    /** Whether the application runs in portable mode. */
    bool isPortable() const;

    /** Directory of the executable, holding read-only bundled material. */
    QString seedDir() const;

    /** Directory for settings and other configuration. */
    QString configDir() const;

    /** Directory for writable application data. */
    QString dataDir() const;

    /** Directory for machine-local data such as caches. */
    QString localDataDir() const;

    /** Directory for the rotating application log file. */
    QString logDir() const;

    /** Default directory for user workspace (.reqdeck) files. */
    QString defaultWorkspacesDir() const;

private:
    AppPaths() = default;

    /** Lazily resolves paths so getters are safe when initialize() was skipped. */
    void ensureInitialized() const;

    bool m_initialized = false;
    bool m_portable = false;
    QString m_seedDir;
    QString m_configDir;
    QString m_dataDir;
    QString m_localDataDir;
    QString m_logDir;
    QString m_defaultWorkspacesDir;
};

#endif // APPPATHS_H
