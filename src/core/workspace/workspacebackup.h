// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKSPACEBACKUP_H
#define WORKSPACEBACKUP_H

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>

/**
 * Keeps one copy of a workspace file and its secrets file per workspace.
 *
 * The copy is made before a file format changes, so the user can go back to the
 * files as they were. Each workspace has a folder named after its UUID holding
 * "workspace.reqdeck" and, when it had secret values, "secrets.json" with the
 * encrypted values. A new copy replaces the old one.
 */
class WorkspaceBackup
{
public:
    /** The files of one copy. */
    struct Copy
    {
        /** Content of the workspace file. */
        QByteArray workspace;
        /** Content of the secrets file; nullopt when the workspace had none. */
        std::optional<QByteArray> secrets;
        /** When the copy was made; invalid for a copy not read from disk. */
        QDateTime created;
    };

    /** Creates a backup store over \a directory, which is created on the first save. */
    explicit WorkspaceBackup(QString directory);

    /** Returns the folder of the copy of \a workspaceId. */
    QString folderPath(const QString &workspaceId) const;

    /** Returns whether a complete copy of \a workspaceId exists. */
    bool exists(const QString &workspaceId) const;

    /** Replaces the copy of \a workspaceId with \a copy; fails for an invalid id. */
    bool save(const QString &workspaceId, const Copy &copy, QString *errorString = nullptr) const;

    /** Returns the copy of \a workspaceId, or nullopt when there is none or it cannot be read. */
    std::optional<Copy> load(const QString &workspaceId) const;

private:
    /** Directory holding one folder per workspace. */
    QString m_directory;
};

#endif // WORKSPACEBACKUP_H
