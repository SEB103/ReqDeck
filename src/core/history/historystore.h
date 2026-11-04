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
    /** Transport failure text; empty when the server answered. */
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
     * Redacts \a entry, prepends it, caps the list, and writes the file. While the
     * file cannot be read the entry stays in memory, the file stays as it is, and
     * false is returned; a later add() merges both.
     */
    bool add(HistoryEntry entry, QString *errorString = nullptr);

    /** Removes all entries and the file. */
    bool clear(QString *errorString = nullptr);

    /** Returns \a spec with literal credentials cleared; {{variable}} references stay. */
    static RequestSpec redacted(const RequestSpec &spec);

private:
    /** Replaces the entries with the content of the file; see load(). */
    bool readFile(QString *errorString);

    /** History file. */
    QString m_filePath;

    /** Entries, newest first. */
    QList<HistoryEntry> m_entries;

    /** Whether the file may hold entries that are not in memory yet. */
    bool m_unread {true};
};

#endif // HISTORYSTORE_H
