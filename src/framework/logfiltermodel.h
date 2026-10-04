// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGFILTERMODEL_H
#define LOGFILTERMODEL_H

#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

#include "framework/diagnosticslevel.h"

/**
 * Severity and text filter in front of LogModel.
 *
 * The log panel shows this proxy so the user can hide routine detail and search
 * the remaining messages without the underlying log losing anything.
 */
class LogFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

    /** Lowest Diagnostics::Level still shown; entries below it are hidden. */
    Q_PROPERTY(int minimumLevel READ minimumLevel WRITE setMinimumLevel
                   NOTIFY minimumLevelChanged)

    /** Case-insensitive substring matched against the message and the category. */
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

    /** Number of entries currently shown. */
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /** Creates the proxy showing every entry from Diagnostics::Info upwards. */
    explicit LogFilterModel(QObject *parent = nullptr);

    /** Returns the lowest severity still shown. */
    int minimumLevel() const { return m_minimumLevel; }

    /** Sets the lowest severity still shown to \a level. */
    void setMinimumLevel(int level);

    /** Returns the current text filter. */
    QString filterText() const { return m_filterText; }

    /** Sets the text filter to \a text and re-evaluates which entries are shown. */
    void setFilterText(const QString &text);

    /** Returns the number of entries currently shown. */
    int count() const { return rowCount(); }

    /** Returns every shown entry as one newline-separated block of text. */
    Q_INVOKABLE QString visibleText() const;

signals:
    /** Emitted when the minimum severity changes. */
    void minimumLevelChanged();

    /** Emitted when the text filter changes. */
    void filterTextChanged();

    /** Emitted when the number of shown entries changes. */
    void countChanged();

protected:
    /** Returns whether \a sourceRow passes the severity and text filters. */
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    /** Lowest Diagnostics::Level still shown. */
    int m_minimumLevel {Diagnostics::Info};

    /** Case-insensitive substring matched against the message and the category. */
    QString m_filterText;
};

#endif // LOGFILTERMODEL_H
