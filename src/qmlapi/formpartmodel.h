// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FORMPARTMODEL_H
#define FORMPARTMODEL_H

#include <QAbstractListModel>
#include <QList>

#include "core/http/requestspec.h"

/** Editable table of the parts of a multipart/form-data body. */
class FormPartModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FormPartModel)

public:
    /** Data roles of a row. */
    enum Role {
        KeyRole = Qt::UserRole + 1, /**< Field name. */
        ValueRole,                  /**< Text, or the path of the file. */
        EnabledRole,                /**< Whether the part is sent. */
        IsFileRole,                 /**< Whether the value is a file path. */
        ContentTypeRole             /**< Content-Type of the part; empty for the default. */
    };
    Q_ENUM(Role)

    /** Creates an empty model. */
    explicit FormPartModel(QObject *parent = nullptr);

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
    void setRows(const QList<FormPart> &rows);

    /** Returns all rows. */
    const QList<FormPart> &rows() const;

    /** Sets \a role of \a row from QML; false for an invalid row or role. */
    Q_INVOKABLE bool setField(int row, int role, const QVariant &value);

    /** Appends an empty enabled text row. */
    Q_INVOKABLE void addRow();

    /** Appends an enabled file row for the file at \a pathOrUrl, named after the file. */
    Q_INVOKABLE void addFile(const QString &pathOrUrl);

    /** Removes \a row; an invalid row number is ignored. */
    Q_INVOKABLE void removeRow(int row);

signals:
    /** Emitted after a change by the user, never by setRows(). */
    void edited();

private:
    /** The rows of the table. */
    QList<FormPart> m_rows;
};

#endif // FORMPARTMODEL_H
