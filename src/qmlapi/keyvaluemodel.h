// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KEYVALUEMODEL_H
#define KEYVALUEMODEL_H

#include <QAbstractListModel>
#include <QList>

#include "core/http/requestspec.h"

/** Editable table of key/value rows, as used for parameters, headers, and form fields. */
class KeyValueModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(KeyValueModel)

public:
    /** Data roles of a row. */
    enum Role {
        KeyRole = Qt::UserRole + 1, /**< Row key. */
        ValueRole,                  /**< Row value. */
        EnabledRole                 /**< Whether the row takes part in the request. */
    };

    /** Creates an empty model. */
    explicit KeyValueModel(QObject *parent = nullptr);

    /** Returns the number of rows. */
    int rowCount(const QModelIndex &parent = {}) const override;

    /** Returns the value of \a role for \a index. */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** Changes \a role of \a index; a changed value emits edited(). */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    /** Returns the item flags of \a index, which are editable. */
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    /** Returns the QML names of the roles. */
    QHash<int, QByteArray> roleNames() const override;

    /** Replaces all rows without emitting edited(). */
    void setRows(const QList<KeyValue> &rows);

    /** Returns all rows. */
    const QList<KeyValue> &rows() const;

    /** Appends an empty enabled row. */
    Q_INVOKABLE void addRow();

    /** Removes \a row; an invalid row number is ignored. */
    Q_INVOKABLE void removeRow(int row);

signals:
    /** Emitted after a change by the user, never by setRows(). */
    void edited();

private:
    /** The rows of the table. */
    QList<KeyValue> m_rows;
};

#endif // KEYVALUEMODEL_H
