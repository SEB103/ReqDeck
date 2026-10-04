// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOGMODEL_H
#define LOGMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>

#include "framework/diagnosticslevel.h"

/**
 * Bounded in-memory log of Qt messages shown by the application's log panel.
 *
 * The model is fed from the installed Qt message handler, which runs on every
 * thread the application uses, so appendEntry() is a slot and must be invoked
 * with a queued connection from anywhere but the GUI thread. Storage is capped:
 * once the cap is reached the oldest entries are dropped in one batch, which
 * keeps a long-running session from growing without bound.
 */
class LogModel : public QAbstractListModel
{
    Q_OBJECT

    /** Number of entries currently held. */
    Q_PROPERTY(int count READ count NOTIFY countChanged)

    /** Absolute path of the log file, or empty when file logging is disabled. */
    Q_PROPERTY(QString logFilePath READ logFilePath WRITE setLogFilePath
                   NOTIFY logFilePathChanged)

public:
    /** Roles exposed to QML log delegates. */
    enum Role {
        /** Local time the entry was recorded, formatted for display. */
        TimestampRole = Qt::UserRole + 1,
        /** Severity as a Diagnostics::Level value. */
        LevelRole,
        /** Short upper-case severity name. */
        LevelNameRole,
        /** Logging category, or an empty string for the default category. */
        CategoryRole,
        /** Message text. */
        MessageRole
    };
    Q_ENUM(Role)

    /** Creates an empty log holding at most \a maximumEntries entries. */
    explicit LogModel(int maximumEntries = 2000, QObject *parent = nullptr);

    /** Returns the number of entries currently held. */
    int count() const { return int(m_entries.size()); }

    /** Returns the maximum number of entries kept. */
    int maximumEntries() const { return m_maximumEntries; }

    /** Returns the absolute path of the log file, or an empty string. */
    QString logFilePath() const { return m_logFilePath; }

    /** Sets the absolute \a path of the log file for the panel's open action. */
    void setLogFilePath(const QString &path);

    /** Returns the display text of the entry at \a row, or an empty string. */
    Q_INVOKABLE QString entryText(int row) const;

    /** Returns data for \a index and \a role. */
    QVariant data(const QModelIndex &index, int role) const override;
    /** Returns the number of rows below \a parent. */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    /** Returns role names exposed to QML. */
    QHash<int, QByteArray> roleNames() const override;

public slots:
    /**
     * Appends one entry.
     *
     * \param level Severity as a Diagnostics::Level value.
     * \param category Logging category; empty for the default category.
     * \param message Message text.
     *
     * Invoke this with a queued connection from any thread but the GUI thread.
     */
    void appendEntry(int level, const QString &category, const QString &message);

    /** Removes every entry. */
    void clear();

signals:
    /** Emitted when the number of entries changes. */
    void countChanged();

    /** Emitted when the log file path changes. */
    void logFilePathChanged();

private:
    /** One recorded log message. */
    struct Entry
    {
        /** Local time the entry was recorded, formatted for display. */
        QString timestamp;
        /** Logging category; empty for the default category. */
        QString category;
        /** Message text. */
        QString message;
        /** Severity as a Diagnostics::Level value. */
        int level {Diagnostics::Info};
    };

    /** Returns the short upper-case name of \a level. */
    static QString levelName(int level);

    /** Entries in arrival order, oldest first. */
    QList<Entry> m_entries;

    /** Maximum number of entries kept before the oldest batch is dropped. */
    int m_maximumEntries;

    /** Absolute path of the log file, or empty when file logging is disabled. */
    QString m_logFilePath;
};

#endif // LOGMODEL_H
