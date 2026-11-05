// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/keyvaluemodel.h"

/*!
 * \brief Creates an empty model.
 * \param parent The QObject parent.
 */
KeyValueModel::KeyValueModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

/*!
 * \brief Returns the number of rows.
 */
int KeyValueModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

/*!
 * \brief Returns the value of \a role for \a index.
 */
QVariant KeyValueModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};

    const KeyValue &row = m_rows.at(index.row());
    switch (role) {
    case KeyRole:
        return row.key;
    case ValueRole:
        return row.value;
    case EnabledRole:
        return row.enabled;
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
bool KeyValueModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return false;

    KeyValue &row = m_rows[index.row()];
    const KeyValue before = row;
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
Qt::ItemFlags KeyValueModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return QAbstractListModel::flags(index);
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

/*!
 * \brief Returns the QML names of the roles.
 */
QHash<int, QByteArray> KeyValueModel::roleNames() const
{
    return {{KeyRole, QByteArrayLiteral("key")},
            {ValueRole, QByteArrayLiteral("value")},
            {EnabledRole, QByteArrayLiteral("enabled")}};
}

/*!
 * \brief Replaces all rows without emitting edited().
 * \param rows The new rows.
 */
void KeyValueModel::setRows(const QList<KeyValue> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

/*!
 * \brief Returns all rows.
 */
const QList<KeyValue> &KeyValueModel::rows() const
{
    return m_rows;
}

/*!
 * \brief Appends an empty enabled row.
 */
void KeyValueModel::addRow()
{
    const int row = int(m_rows.size());
    beginInsertRows({}, row, row);
    m_rows.append(KeyValue {});
    endInsertRows();
    emit edited();
}

/*!
 * \brief Removes \a row; an invalid row number is ignored.
 * \param row The row number.
 */
void KeyValueModel::removeRow(int row)
{
    if (row < 0 || row >= m_rows.size())
        return;

    beginRemoveRows({}, row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    emit edited();
}
