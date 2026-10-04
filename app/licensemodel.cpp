// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Directory-scanning list model for bundled license documents.
 */

#include "licensemodel.h"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

/*!
 * \brief Creates an empty model.
 */
LicenseModel::LicenseModel(QObject *parent)
    : QAbstractListModel(parent)
{}

/*!
 * \brief Returns data for \a index and \a role.
 */
QVariant LicenseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TitleRole:
        return entry.title;
    case PathRole:
        return entry.path;
    default:
        return {};
    }
}

/*!
 * \brief Returns the number of license documents.
 */
int LicenseModel::rowCount(const QModelIndex &parent) const
{
    // A list model is flat: only the invisible root has children.
    if (parent.isValid())
        return 0;
    return int(m_entries.size());
}

/*!
 * \brief Returns the role names exposed to QML.
 */
QHash<int, QByteArray> LicenseModel::roleNames() const
{
    return {{TitleRole, QByteArrayLiteral("title")},
            {PathRole, QByteArrayLiteral("path")}};
}

/*!
 * \brief Scans \a directory for "*.txt" license files and rebuilds the model.
 *
 * A "qrc:/" prefix is rewritten to the ":/" resource root so the same call works
 * for embedded resources and for a filesystem directory. Entries are ordered by
 * file name, case-insensitively; the title is the file's complete base name,
 * which for the bundled files is the SPDX license identifier.
 */
void LicenseModel::setDirectory(const QString &directory)
{
    beginResetModel();
    m_entries.clear();

    QString path = directory;
    if (path.startsWith(QStringLiteral("qrc:/")))
        path.remove(0, 3); // "qrc:/..." -> ":/..."

    const QDir dir(path);
    const QFileInfoList files =
        dir.entryInfoList({QStringLiteral("*.txt")}, QDir::Files | QDir::Readable,
                          QDir::Name | QDir::IgnoreCase);

    m_entries.reserve(files.size());
    for (const QFileInfo &fileInfo : files)
        m_entries.push_back({fileInfo.completeBaseName(), fileInfo.absoluteFilePath()});

    endResetModel();
}

/*!
 * \brief Returns the file path of the entry at \a index, or an empty string.
 */
QString LicenseModel::pathAt(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return {};
    return m_entries.at(index).path;
}

/*!
 * \brief Returns the title of the entry at \a index, or an empty string.
 */
QString LicenseModel::titleAt(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return {};
    return m_entries.at(index).title;
}
