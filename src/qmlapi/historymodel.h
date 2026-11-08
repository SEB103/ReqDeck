// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HISTORYMODEL_H
#define HISTORYMODEL_H

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include "core/history/historystore.h"

class CollectionTreeModel;
struct ResponseData;

/** The history of executed requests, newest first, exposed to QML as \c cppHistory. */
class HistoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HistoryModel)
    Q_MOC_INCLUDE("qmlapi/collectiontreemodel.h")

    /** The entries that match filterText, for the list view. */
    Q_PROPERTY(QAbstractItemModel *filtered READ filtered CONSTANT)

    /** Words an entry must all contain, in method, URL, status, or error; empty shows all. */
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    /** Data roles of a row. */
    enum Role {
        IdRole = Qt::UserRole + 1, /**< Stable identifier. */
        MethodRole,                /**< HTTP method. */
        UrlRole,                   /**< URL as edited, with variables unresolved. */
        StatusCodeRole,            /**< HTTP status code, or 0 without a response. */
        ElapsedRole,               /**< Duration in milliseconds. */
        ErrorRole,                 /**< Transport failure text, or empty. */
        TimeRole,                  /**< Local time of the request as text. */
        BodyOmittedRole,           /**< Whether the raw body was too large to keep. */
        SearchRole                 /**< Method, URL, status, and error as one text, for the search. */
    };

    /** Creates an empty model over the history file \a filePath; see load(). */
    explicit HistoryModel(const QString &filePath, QObject *parent = nullptr);

    /** Returns the number of entries. */
    int rowCount(const QModelIndex &parent = {}) const override;

    /** Returns the value of \a role for \a index. */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** Returns the QML names of the roles. */
    QHash<int, QByteArray> roleNames() const override;

    /** Reads the history file. */
    void load();

    /** Adds a finished \a request, unless the user cancelled it. */
    void record(const RequestSpec &request, const ResponseData &response);

    /** Removes all entries and the file. */
    Q_INVOKABLE void clear();

    /** Adds the entry at \a row to \a collection as a new top-level request; returns its id or "". */
    Q_INVOKABLE QString openEntry(int row, CollectionTreeModel *collection);

    /** Returns the entries that match filterText. */
    QAbstractItemModel *filtered() const;

    /** Returns the search words. */
    QString filterText() const;

    /** Sets the search words to \a text. */
    void setFilterText(const QString &text);

    /** Returns the row of this model shown at \a filteredRow of filtered(), or -1. */
    Q_INVOKABLE int sourceRow(int filteredRow) const;

    /** Returns the request of \a row, or an empty one for an invalid row. */
    RequestSpec requestAt(int row) const;

    /** Returns a name for a request made from \a row, or an empty string. */
    QString displayName(int row) const;

signals:
    /** Emitted when the search words change. */
    void filterTextChanged();

    /** Emitted once when the history file fails, and again only after it worked. */
    void storageFailed(const QString &message);

private:
    /** Applies the outcome of a store operation to the failure report. */
    void reportResult(bool ok, const QString &errorString);

    /** The entries and their file. */
    HistoryStore m_store;

    /** Whether the current failure was reported. */
    bool m_failureReported {false};

    /** The search words. */
    QString m_filterText;

    /** The entries that match the search words. */
    QSortFilterProxyModel *m_filtered {nullptr};
};

#endif // HISTORYMODEL_H
