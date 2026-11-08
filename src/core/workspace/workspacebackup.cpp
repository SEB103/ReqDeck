// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/workspacebackup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <utility>

#include "core/workspace/jsonfile.h"
#include "core/workspace/workspacedata.h"

namespace {
/*!
 * \internal
 * \brief File name of the workspace file in a copy.
 */
constexpr QLatin1StringView kWorkspaceFile("workspace.reqdeck");

/*!
 * \internal
 * \brief File name of the secrets file in a copy.
 */
constexpr QLatin1StringView kSecretsFile("secrets.json");

/*!
 * \internal
 * \brief Returns the content of \a path, or std::nullopt when it cannot be read.
 */
std::optional<QByteArray> readWhole(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return file.readAll();
}
} // namespace

/*!
 * \brief Creates a backup store over \a directory.
 * \param directory The directory holding one folder per workspace.
 */
WorkspaceBackup::WorkspaceBackup(QString directory)
    : m_directory(std::move(directory))
{
}

/*!
 * \brief Returns the folder of the copy of \a workspaceId.
 * \param workspaceId The UUID of the workspace.
 */
QString WorkspaceBackup::folderPath(const QString &workspaceId) const
{
    return QDir(m_directory).filePath(workspaceId);
}

/*!
 * \brief Returns whether a complete copy of \a workspaceId exists.
 *
 * The workspace file is written last, so its presence marks a complete copy.
 * \param workspaceId The UUID of the workspace.
 */
bool WorkspaceBackup::exists(const QString &workspaceId) const
{
    return isValidWorkspaceId(workspaceId)
           && QFileInfo::exists(QDir(folderPath(workspaceId)).filePath(kWorkspaceFile));
}

/*!
 * \brief Replaces the copy of \a workspaceId with \a copy.
 *
 * The old workspace file goes first and the new one is written last, so an
 * interrupted save never leaves a workspace file beside the secrets of another
 * copy.
 * \param workspaceId The UUID of the workspace; anything else fails, so the id
 *        cannot point outside the store.
 * \param copy The files to keep; \c created is ignored.
 * \param errorString Receives the reason on failure when it is not null.
 * \return True on success.
 */
bool WorkspaceBackup::save(const QString &workspaceId, const Copy &copy, QString *errorString) const
{
    const auto fail = [errorString] {
        if (errorString)
            *errorString = QStringLiteral("A copy of the workspace could not be written.");
        return false;
    };
    if (!isValidWorkspaceId(workspaceId))
        return fail();

    const QDir folder(folderPath(workspaceId));
    const QString workspacePath = folder.filePath(kWorkspaceFile);
    const QString secretsPath = folder.filePath(kSecretsFile);
    if (QFile::exists(workspacePath) && !QFile::remove(workspacePath))
        return fail();
    if (copy.secrets) {
        if (!writeFileAtomically(secretsPath, *copy.secrets))
            return fail();
    } else if (QFile::exists(secretsPath) && !QFile::remove(secretsPath)) {
        return fail();
    }
    return writeFileAtomically(workspacePath, copy.workspace) || fail();
}

/*!
 * \brief Returns when the copy of \a workspaceId was made, without reading it.
 * \param workspaceId The UUID of the workspace.
 */
QDateTime WorkspaceBackup::created(const QString &workspaceId) const
{
    if (!exists(workspaceId))
        return {};
    return QFileInfo(QDir(folderPath(workspaceId)).filePath(kWorkspaceFile)).lastModified();
}

/*!
 * \brief Returns the copy of \a workspaceId.
 * \param workspaceId The UUID of the workspace.
 * \return The copy, or std::nullopt when there is none or a file of it cannot be read.
 */
std::optional<WorkspaceBackup::Copy> WorkspaceBackup::load(const QString &workspaceId) const
{
    if (!exists(workspaceId))
        return std::nullopt;

    const QDir folder(folderPath(workspaceId));
    const QString workspacePath = folder.filePath(kWorkspaceFile);
    const QString secretsPath = folder.filePath(kSecretsFile);
    Copy copy;
    const std::optional<QByteArray> workspace = readWhole(workspacePath);
    if (!workspace)
        return std::nullopt;
    copy.workspace = *workspace;
    if (QFileInfo::exists(secretsPath)) {
        copy.secrets = readWhole(secretsPath);
        if (!copy.secrets)
            return std::nullopt;
    }
    copy.created = QFileInfo(workspacePath).lastModified();
    return copy;
}
