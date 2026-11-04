// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/jsonfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

/*!
 * \brief Writes \a content to \a path.
 *
 * QSaveFile keeps the previous file when the write fails.
 * \param path The target file.
 * \param content The file content.
 * \return True on success.
 */
bool writeFileAtomically(const QString &path, const QByteArray &content)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(content);
    return file.commit();
}

/*!
 * \brief Writes \a root as indented JSON to \a path.
 * \param path The target file.
 * \param root The JSON content.
 * \return True on success.
 */
bool writeJsonFile(const QString &path, const QJsonObject &root)
{
    return writeFileAtomically(path, QJsonDocument(root).toJson(QJsonDocument::Indented));
}

/*!
 * \brief Moves the unusable file at \a path aside.
 *
 * The next write must not overwrite data the user may still recover by hand.
 * The file moves to a temporary name first, so an older copy is only replaced
 * once the new one is safe; if that last step fails the file stays under the
 * temporary name and is still out of the way.
 * \param path The file to rename.
 * \return False when the file could not be moved; the caller must not overwrite it.
 */
bool setAsideCorruptFile(const QString &path)
{
    const QString target = path + QStringLiteral(".corrupt");
    const QString temporary = target + QStringLiteral(".new");

    QFile::remove(temporary);
    if (!QFile::rename(path, temporary))
        return false;

    QFile::remove(target);
    QFile::rename(temporary, target);
    return true;
}
