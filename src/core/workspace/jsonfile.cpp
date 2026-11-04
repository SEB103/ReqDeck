// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/jsonfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

/*!
 * \brief Writes \a root as indented JSON to \a path.
 *
 * QSaveFile keeps the previous file when the write fails.
 * \param path The target file.
 * \param root The JSON content.
 * \return True on success.
 */
bool writeJsonFile(const QString &path, const QJsonObject &root)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

/*!
 * \brief Moves the unusable file at \a path aside.
 *
 * The next write must not overwrite data the user may still recover by hand.
 * \param path The file to rename.
 */
void setAsideCorruptFile(const QString &path)
{
    const QString target = path + QStringLiteral(".corrupt");
    QFile::remove(target);
    QFile::rename(path, target);
}
