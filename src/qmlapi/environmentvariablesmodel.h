// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ENVIRONMENTVARIABLESMODEL_H
#define ENVIRONMENTVARIABLESMODEL_H

#include <QAbstractListModel>
#include <QList>

#include "core/workspace/workspacedata.h"

/** Editable table of the variables of one environment. */
class EnvironmentVariablesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EnvironmentVariablesModel)

public:
    /** Data roles of a row. */
    enum Role {
        KeyRole = Qt::UserRole + 1, /**< Variable name. */
        ValueRole,                  /**< Variable value. */
        EnabledRole,                /**< Whether the variable takes part in substitution. */
        SecretRole                  /**< Whether the value stays out of the workspace file. */
    };

    /** Creates an empty model. */
    explicit EnvironmentVariablesModel(QObject *parent = nullptr);

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
    void setRows(const QList<EnvironmentVariable> &rows);

    /** Returns all rows. */
    const QList<EnvironmentVariable> &rows() const;

    /** Appends an empty enabled row. */
    Q_INVOKABLE void addRow();

    /** Removes \a row; an invalid row number is ignored. */
    Q_INVOKABLE void removeRow(int row);

signals:
    /** Emitted after a change by the user, never by setRows(). */
    void edited();

private:
    /** The rows of the table. */
    QList<EnvironmentVariable> m_rows;
};

#endif // ENVIRONMENTVARIABLESMODEL_H
