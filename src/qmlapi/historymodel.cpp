// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/historymodel.h"

#include "core/http/responsedata.h"
#include "core/workspace/collectiontree.h"
#include "qmlapi/collectiontreemodel.h"

/*!
 * \brief Creates an empty model over the history file \a filePath.
 *
 * The file is read by load(), so that a listener of storageFailed() can connect
 * first.
 * \param filePath The history file.
 * \param parent The QObject parent.
 */
HistoryModel::HistoryModel(const QString &filePath, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(filePath)
{
}

/*!
 * \brief Returns the number of entries.
 */
int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_store.entries().size());
}

/*!
 * \brief Returns the value of \a role for \a index.
 */
QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};

    const HistoryEntry &entry = m_store.entries().at(index.row());
    switch (role) {
    case IdRole:
        return entry.id;
    case MethodRole:
        return QString::fromLatin1(httpMethodVerb(entry.request.method));
    case UrlRole:
        return entry.request.url;
    case StatusCodeRole:
        return entry.statusCode;
    case ElapsedRole:
        return entry.elapsedMs;
    case ErrorRole:
        return entry.errorString;
    case TimeRole:
        return entry.timestamp.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    default:
        return {};
    }
}

/*!
 * \brief Returns the QML names of the roles.
 */
QHash<int, QByteArray> HistoryModel::roleNames() const
{
    return {{IdRole, QByteArrayLiteral("entryId")},
            {MethodRole, QByteArrayLiteral("method")},
            {UrlRole, QByteArrayLiteral("url")},
            {StatusCodeRole, QByteArrayLiteral("statusCode")},
            {ElapsedRole, QByteArrayLiteral("elapsedMs")},
            {ErrorRole, QByteArrayLiteral("errorString")},
            {TimeRole, QByteArrayLiteral("timeText")}};
}

/*!
 * \brief Reads the history file.
 *
 * A file that fails is reported through storageFailed(); the list then starts
 * empty and the application works on.
 */
void HistoryModel::load()
{
    QString errorString;
    beginResetModel();
    const bool ok = m_store.load(&errorString);
    endResetModel();
    reportResult(ok, errorString);
}

/*!
 * \brief Adds a finished \a request.
 *
 * Rows are reported one by one, so a list being read keeps its place and its
 * current entry. Only when the store merges entries it could not read before does
 * the model reset. The store writes on the GUI thread, which took about 200 ms
 * for 200 entries of maximum size, depending on the machine and on the content.
 * \param request The request as it was sent; the store redacts it.
 * \param response The outcome; a request the user cancelled is left out.
 */
void HistoryModel::record(const RequestSpec &request, const ResponseData &response)
{
    if (response.error == ResponseError::Aborted)
        return;

    HistoryEntry entry;
    entry.request = request;
    entry.statusCode = response.statusCode;
    entry.elapsedMs = response.elapsedMs;
    entry.errorString = response.errorString;

    // Another running instance may have changed the file; then the merged list
    // replaces the rows instead of one inserted row.
    HistoryStore::Update update = m_store.prepareAdd(std::move(entry));
    const bool ok = update.ok;
    const QString errorString = update.errorString;
    if (update.prependsOne) {
        if (rowCount() >= HistoryStore::kMaxEntries) {
            beginRemoveRows({}, rowCount() - 1, rowCount() - 1);
            m_store.dropOldest();
            endRemoveRows();
        }
        beginInsertRows({}, 0, 0);
        m_store.apply(std::move(update));
        endInsertRows();
    } else {
        beginResetModel();
        m_store.apply(std::move(update));
        endResetModel();
    }
    reportResult(ok, errorString);
}

/*!
 * \brief Removes all entries and the file.
 */
void HistoryModel::clear()
{
    QString errorString;
    beginResetModel();
    const bool ok = m_store.clear(&errorString);
    endResetModel();
    reportResult(ok, errorString);
}

/*!
 * \brief Adds the entry at \a row to \a collection as a new top-level request.
 * \param row The row.
 * \param collection The collection of the open workspace.
 * \return The id of the new request, or an empty string for an invalid row.
 */
QString HistoryModel::openEntry(int row, CollectionTreeModel *collection)
{
    if (row < 0 || row >= rowCount())
        return {};
    return collection->addRequestFrom({}, requestAt(row), displayName(row));
}

/*!
 * \brief Returns the request of \a row.
 * \param row The row.
 * \return The redacted request, or an empty one for an invalid row.
 */
RequestSpec HistoryModel::requestAt(int row) const
{
    return row >= 0 && row < rowCount() ? m_store.entries().at(row).request : RequestSpec {};
}

/*!
 * \brief Returns a name for a request made from \a row.
 * \param row The row.
 * \return The name, or an empty string for an invalid row.
 */
QString HistoryModel::displayName(int row) const
{
    return row >= 0 && row < rowCount() ? requestDisplayName(m_store.entries().at(row).request)
                                        : QString();
}

/*!
 * \internal
 * \brief Applies the outcome of a store operation to the failure report.
 *
 * A file that cannot be written fails on every request, and one report is enough.
 * \param ok Whether the operation worked.
 * \param errorString The reason when it did not.
 */
void HistoryModel::reportResult(bool ok, const QString &errorString)
{
    if (ok) {
        m_failureReported = false;
    } else if (!m_failureReported) {
        m_failureReported = true;
        emit storageFailed(errorString);
    }
}
