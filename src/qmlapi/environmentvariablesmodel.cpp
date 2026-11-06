// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/environmentvariablesmodel.h"

/*!
 * \brief Creates an empty model.
 * \param parent The QObject parent.
 */
EnvironmentVariablesModel::EnvironmentVariablesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

/*!
 * \brief Returns the number of rows.
 */
int EnvironmentVariablesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

/*!
 * \brief Returns the value of \a role for \a index.
 */
QVariant EnvironmentVariablesModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};

    const EnvironmentVariable &row = m_rows.at(index.row());
    switch (role) {
    case KeyRole:
        return row.key;
    case ValueRole:
        return row.value;
    case EnabledRole:
        return row.enabled;
    case SecretRole:
        return row.secret;
    default:
        return {};
    }
}

/*!
 * \brief Changes \a role of \a index.
 * \param index The row.
 * \param value The new value.
 * \param role The role to change.
 * \return False for an invalid index or an unknown role.
 */
bool EnvironmentVariablesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return false;

    EnvironmentVariable &row = m_rows[index.row()];
    const EnvironmentVariable before = row;
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
    case SecretRole:
        row.secret = value.toBool();
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
Qt::ItemFlags EnvironmentVariablesModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return QAbstractListModel::flags(index);
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

/*!
 * \brief Returns the QML names of the roles.
 */
QHash<int, QByteArray> EnvironmentVariablesModel::roleNames() const
{
    return {{KeyRole, QByteArrayLiteral("key")},
            {ValueRole, QByteArrayLiteral("value")},
            {EnabledRole, QByteArrayLiteral("enabled")},
            {SecretRole, QByteArrayLiteral("secret")}};
}

/*!
 * \brief Replaces all rows without emitting edited().
 * \param rows The new rows.
 */
void EnvironmentVariablesModel::setRows(const QList<EnvironmentVariable> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

/*!
 * \brief Returns all rows.
 */
const QList<EnvironmentVariable> &EnvironmentVariablesModel::rows() const
{
    return m_rows;
}

/*!
 * \brief Appends an empty enabled row.
 */
void EnvironmentVariablesModel::addRow()
{
    const int row = int(m_rows.size());
    beginInsertRows({}, row, row);
    m_rows.append(EnvironmentVariable {});
    endInsertRows();
    emit edited();
}

/*!
 * \brief Removes \a row; an invalid row number is ignored.
 * \param row The row number.
 */
void EnvironmentVariablesModel::removeRow(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;

    beginRemoveRows({}, row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    emit edited();
}
