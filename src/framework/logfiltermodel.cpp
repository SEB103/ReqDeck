// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "framework/logfiltermodel.h"

#include "framework/logmodel.h"

/*!
 * \property LogFilterModel::minimumLevel
 * \brief Lowest Diagnostics::Level still shown.
 */

/*!
 * \property LogFilterModel::filterText
 * \brief Case-insensitive substring matched against the message and the category.
 */

/*!
 * \property LogFilterModel::count
 * \brief Number of entries currently shown.
 */

/*!
 * \brief Creates the proxy and keeps the shown-entry count published.
 */
LogFilterModel::LogFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);

    connect(this, &QAbstractItemModel::rowsInserted, this, &LogFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &LogFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &LogFilterModel::countChanged);
}

/*!
 * \brief Sets the lowest severity still shown to \a level.
 */
void LogFilterModel::setMinimumLevel(int level)
{
    const int clamped = qBound(int(Diagnostics::Debug), level, int(Diagnostics::Error));
    if (m_minimumLevel == clamped)
        return;

    m_minimumLevel = clamped;
    invalidateRowsFilter();
    emit minimumLevelChanged();
    emit countChanged();
}

/*!
 * \brief Sets the text filter to \a text.
 */
void LogFilterModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;

    m_filterText = text;
    invalidateRowsFilter();
    emit filterTextChanged();
    emit countChanged();
}

/*!
 * \brief Returns every shown entry as one newline-separated block of text.
 */
QString LogFilterModel::visibleText() const
{
    const auto *source = qobject_cast<const LogModel *>(sourceModel());
    if (!source)
        return {};

    QStringList lines;
    const int rows = rowCount();
    lines.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex sourceIndex = mapToSource(index(row, 0));
        if (sourceIndex.isValid())
            lines.append(source->entryText(sourceIndex.row()));
    }

    return lines.join(QLatin1Char('\n'));
}

/*!
 * \brief Returns whether \a sourceRow passes the severity and text filters.
 */
bool LogFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *model = sourceModel();
    if (!model)
        return true;

    const QModelIndex sourceIndex = model->index(sourceRow, 0, sourceParent);
    if (model->data(sourceIndex, LogModel::LevelRole).toInt() < m_minimumLevel)
        return false;

    if (m_filterText.isEmpty())
        return true;

    return model->data(sourceIndex, LogModel::MessageRole).toString()
               .contains(m_filterText, Qt::CaseInsensitive)
           || model->data(sourceIndex, LogModel::CategoryRole).toString()
                  .contains(m_filterText, Qt::CaseInsensitive);
}
