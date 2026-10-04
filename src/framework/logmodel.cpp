// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "framework/logmodel.h"

#include <QDateTime>

namespace {

/*!
 * \internal
 * \brief Fraction of the capacity dropped when the log is full.
 *
 * Trimming a batch instead of a single entry keeps the amortized cost of a log
 * message constant; removing one row per message would move the whole backing
 * array on every message once the cap is reached.
 */
constexpr int kTrimDivisor = 10;

} // namespace

/*!
 * \property LogModel::count
 * \brief Number of entries currently held.
 */

/*!
 * \property LogModel::logFilePath
 * \brief Absolute path of the log file, or empty when file logging is disabled.
 */

/*!
 * \brief Creates an empty log.
 * \param maximumEntries Maximum number of entries kept; clamped to at least 10.
 * \param parent Optional QObject parent.
 */
LogModel::LogModel(int maximumEntries, QObject *parent)
    : QAbstractListModel(parent)
    , m_maximumEntries(qMax(10, maximumEntries))
{}

/*!
 * \brief Sets the absolute \a path of the log file.
 */
void LogModel::setLogFilePath(const QString &path)
{
    if (m_logFilePath == path)
        return;

    m_logFilePath = path;
    emit logFilePathChanged();
}

/*!
 * \brief Returns the display text of the entry at \a row.
 */
QString LogModel::entryText(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(row);
    if (entry.category.isEmpty())
        return QStringLiteral("%1 [%2] %3").arg(entry.timestamp, levelName(entry.level),
                                                entry.message);

    return QStringLiteral("%1 [%2] %3: %4").arg(entry.timestamp, levelName(entry.level),
                                                entry.category, entry.message);
}

/*!
 * \brief Appends one entry.
 * \param level Severity as a Diagnostics::Level value.
 * \param category Logging category; empty for the default category.
 * \param message Message text.
 */
void LogModel::appendEntry(int level, const QString &category, const QString &message)
{
    if (m_entries.size() >= m_maximumEntries) {
        const int trimCount = qMax(1, m_maximumEntries / kTrimDivisor);
        beginRemoveRows(QModelIndex(), 0, trimCount - 1);
        m_entries.remove(0, trimCount);
        endRemoveRows();
    }

    Entry entry;
    entry.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    entry.category = category == QLatin1String("default") ? QString() : category;
    entry.message = message;
    entry.level = level;

    const int row = int(m_entries.size());
    beginInsertRows(QModelIndex(), row, row);
    m_entries.push_back(std::move(entry));
    endInsertRows();

    emit countChanged();
}

/*!
 * \brief Removes every entry.
 */
void LogModel::clear()
{
    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();

    emit countChanged();
}

/*!
 * \brief Returns the short upper-case name of \a level.
 */
QString LogModel::levelName(int level)
{
    switch (level) {
    case Diagnostics::Debug: return QStringLiteral("DEBUG");
    case Diagnostics::Info: return QStringLiteral("INFO");
    case Diagnostics::Warning: return QStringLiteral("WARNING");
    case Diagnostics::Error: return QStringLiteral("ERROR");
    default: return QStringLiteral("INFO");
    }
}

/*!
 * \brief Returns model data for \a index and \a role.
 */
QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case MessageRole: return entry.message;
    case TimestampRole: return entry.timestamp;
    case LevelRole: return entry.level;
    case LevelNameRole: return levelName(entry.level);
    case CategoryRole: return entry.category;
    default: return {};
    }
}

/*!
 * \brief Returns the number of rows below \a parent.
 */
int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return int(m_entries.size());
}

/*!
 * \brief Returns the role names exposed to QML.
 */
QHash<int, QByteArray> LogModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {TimestampRole, "timestamp"},
        {LevelRole, "level"},
        {LevelNameRole, "levelName"},
        {CategoryRole, "category"},
        {MessageRole, "message"}
    };
}
