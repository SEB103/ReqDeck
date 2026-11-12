// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RESPONSECACHE_H
#define RESPONSECACHE_H

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

struct ResponseData;

/**
 * Last response of each request, kept in memory for response() templates.
 *
 * Nothing is written to disk: responses can hold tokens. Only a JSON body of at
 * most kMaxJsonBytes is kept, parsed once; of any other response a template can
 * read only the headers.
 */
class ResponseCache
{
public:
    /** Largest JSON body kept for templates. */
    static constexpr qsizetype kMaxJsonBytes = 4 * 1024 * 1024;

    /** Keeps \a response as the last one of the request \a requestId. */
    void store(const QString &requestId, const ResponseData &response);

    /** Forgets every response, for example when another workspace opens. */
    void clear();

    /** Returns whether no response is kept. */
    bool isEmpty() const;

    /** Returns whether a response of the request \a requestId is kept. */
    bool contains(const QString &requestId) const;

    /**
     * Returns the value \a selector picks from the newest kept response of the
     * requests \a requestIds, or nothing.
     */
    std::optional<QString> value(const QStringList &requestIds, const QString &selector) const;

    /** Returns the ids of the requests with a kept response, newest first. */
    QStringList requestIds() const;

    /** Forgets the responses of requests not in \a requestIds, for example deleted ones. */
    void retain(const QSet<QString> &requestIds);

private:
    /** One kept response. */
    struct Entry
    {
        /** Response headers as received. */
        QList<std::pair<QByteArray, QByteArray>> headers;
        /** The JSON body, parsed; undefined when the body was not JSON or too large. */
        QJsonValue json {QJsonValue::Undefined};
        /** Order of storing; higher is newer. */
        quint64 order {0};
    };

    /** Kept responses by request id. */
    QHash<QString, Entry> m_entries;

    /** Order given to the next stored response. */
    quint64 m_nextOrder {0};
};

#endif // RESPONSECACHE_H
