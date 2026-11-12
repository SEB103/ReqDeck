// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/formpartmodel.h"

#include <QFileInfo>
#include <QUrl>

/*!
 * \class FormPartModel
 * \inmodule ReqDeckQmlApi
 * \brief Editable table of the parts of a multipart/form-data body.
 */

/*!
 * \brief Creates an empty model owned by \a parent.
 */
FormPartModel::FormPartModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

/*!
 * \brief Returns the number of rows.
 */
int FormPartModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

/*!
 * \brief Returns the value of \a role for \a index.
 */
QVariant FormPartModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const FormPart &row = m_rows.at(index.row());
    switch (role) {
    case KeyRole:
        return row.key;
    case ValueRole:
        return row.value;
    case EnabledRole:
        return row.enabled;
    case IsFileRole:
        return row.isFile;
    case ContentTypeRole:
        return row.contentType;
    default:
        return {};
    }
}

/*!
 * \brief Changes \a role of \a index to \a value.
 * \return False for an invalid index or an unknown role.
 */
bool FormPartModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return false;
    FormPart &row = m_rows[index.row()];
    const FormPart before = row;
    switch (role) {
    case KeyRole:
        row.key = value.toString();
        break;
    case ValueRole:
        row.value = value.toString();
        break;
    case EnabledRole:
        row.enabled = value.toBool();
        break;
    case IsFileRole:
        row.isFile = value.toBool();
        break;
    case ContentTypeRole:
        row.contentType = value.toString();
        break;
    default:
        return false;
    }
    if (row != before) {
        emit dataChanged(index, index, {role});
        emit edited();
    }
    return true;
}

/*!
 * \brief Returns the item flags of \a index; rows are editable.
 */
Qt::ItemFlags FormPartModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return QAbstractListModel::flags(index);
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

/*!
 * \brief Returns the QML names of the roles.
 */
QHash<int, QByteArray> FormPartModel::roleNames() const
{
    return {{KeyRole, QByteArrayLiteral("key")},
            {ValueRole, QByteArrayLiteral("value")},
            {EnabledRole, QByteArrayLiteral("enabled")},
            {IsFileRole, QByteArrayLiteral("isFile")},
            {ContentTypeRole, QByteArrayLiteral("contentType")}};
}

/*!
 * \brief Replaces all rows with \a rows without emitting edited().
 */
void FormPartModel::setRows(const QList<FormPart> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

/*!
 * \brief Returns all rows.
 */
const QList<FormPart> &FormPartModel::rows() const
{
    return m_rows;
}

/*!
 * \brief Sets \a role of \a row to \a value, as QML delegates do.
 * \return False for an invalid row or role.
 */
bool FormPartModel::setField(int row, int role, const QVariant &value)
{
    return setData(index(row), value, role);
}

/*!
 * \brief Appends an empty enabled text row.
 */
void FormPartModel::addRow()
{
    const int row = int(m_rows.size());
    beginInsertRows({}, row, row);
    m_rows.append(FormPart {});
    endInsertRows();
    emit edited();
}

/*!
 * \brief Appends a file row for the file at \a pathOrUrl.
 *
 * The field is named after the file without its suffix, as a start.
 * \param pathOrUrl A local path or a file URL, as a file dialog gives it.
 */
void FormPartModel::addFile(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    if (path.trimmed().isEmpty())
        return;
    FormPart part;
    part.key = QFileInfo(path).completeBaseName();
    part.value = path;
    part.isFile = true;
    const int row = int(m_rows.size());
    beginInsertRows({}, row, row);
    m_rows.append(part);
    endInsertRows();
    emit edited();
}

/*!
 * \brief Removes \a row; an invalid row number is ignored.
 */
void FormPartModel::removeRow(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;
    beginRemoveRows({}, row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    emit edited();
}
