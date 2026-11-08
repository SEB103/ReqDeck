// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QDateTime>
#include <QList>
#include <QString>

#include "core/http/requestspec.h"

/** One executed request; the response body is never kept. */
struct HistoryEntry
{
    /** Stable identifier; assigned by HistoryStore::add() when empty. */
    QString id;
    /** Time of the request; assigned by HistoryStore::add() when invalid. */
    QDateTime timestamp;
    /** The request as edited, with variables unresolved and credentials redacted. */
    RequestSpec request;
    /** HTTP status code; 0 when no response was received. */
    int statusCode {0};
    /** Duration of the request in milliseconds. */
    qint64 elapsedMs {0};
    /** Transport failure text, also after a status line; empty when the transfer completed. */
    QString errorString;
    /** Whether the raw body was too large to keep. */
    bool bodyOmitted {false};

    bool operator==(const HistoryEntry &) const = default;
};

/** Application-wide history of executed requests in one JSON file, newest first. */
class HistoryStore
{
public:
    /** Maximum number of entries kept. */
    static constexpr int kMaxEntries = 200;

    /** Longest raw request body kept, because the file is rewritten on every add. */
    static constexpr qsizetype kMaxStoredBodyChars = 65536;

    /** Longest text kept in any other field of an entry; longer text is cut. */
    static constexpr qsizetype kMaxStoredValueChars = 8192;

    /** What an add wrote, for apply(). */
    struct Update
    {
        /** Whether the file was read and written. */
        bool ok {false};
        /** Reason on failure; empty on success. */
        QString errorString;
        /** The entries after the add, newest first. */
        QList<HistoryEntry> entries;
        /** Whether entries are the previous ones with only the new one in front. */
        bool prependsOne {true};
    };

    /** Creates a store over \a filePath; add() reads existing entries if load() was not called. */
    explicit HistoryStore(QString filePath);

    /**
     * Reads the file; a missing file gives an empty history. A malformed file is
     * renamed to "<file>.corrupt". Returns false with \a errorString set for a
     * malformed or unreadable file.
     */
    bool load(QString *errorString = nullptr);

    /** Returns the entries, newest first. */
    const QList<HistoryEntry> &entries() const;

    /**
     * Redacts and caps \a entry and writes it in front of the entries the file holds
     * now, which may include those of another running instance. The list in memory
     * changes only with apply(). While the file cannot be read or locked, nothing is
     * written and the entry is kept for the next add.
     */
    Update prepareAdd(HistoryEntry entry);

    /** Makes the entries of \a update the list in memory. */
    void apply(Update update);

    /** Runs prepareAdd() and apply(); returns whether the file was written. */
    bool add(HistoryEntry entry, QString *errorString = nullptr);

    /** Removes all entries, the file, and a set-aside corrupt file. */
    bool clear(QString *errorString = nullptr);

    /** Returns whether the file may hold entries that add() will merge into the list. */
    bool hasUnreadFile() const;

    /** Removes the oldest entry from the list; the file changes with the next add(). */
    void dropOldest();

    /**
     * Returns \a spec with literal credentials cleared in auth fields, the URL, rows
     * named like credentials, and JSON or form raw bodies; {{variable}} references
     * stay. XML and other raw text are kept.
     */
    static RequestSpec redacted(const RequestSpec &spec);

private:
    /** Outcome of reading the file. */
    enum class FileState {
        /** The file was read, or there is none. */
        Read,
        /** The file was malformed and set aside; it reads as empty. */
        SetAside,
        /** The file could neither be read nor set aside. */
        Unreadable
    };

    /** Reads the entries of the file into \a entries. */
    FileState readEntries(QList<HistoryEntry> &entries) const;

    /** History file. */
    QString m_filePath;

    /** Entries, newest first. */
    QList<HistoryEntry> m_entries;

    /** Whether the file may hold entries that are not in memory yet. */
    bool m_unread {true};

    /** Entries of this store not yet written, because the file could not be read. */
    QList<HistoryEntry> m_pending;
};

#endif // HISTORYSTORE_H
