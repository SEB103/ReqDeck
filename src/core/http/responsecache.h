// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RESPONSECACHE_H
#define RESPONSECACHE_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

struct ResponseData;

/**
 * Last response of each request, kept in memory for response() templates.
 *
 * Nothing is written to disk: responses can hold tokens. A body that did not
 * fit in memory is not kept, so a template can read only its headers.
 */
class ResponseCache
{
public:
    /** Keeps \a response as the last one of the request \a requestId named \a requestName. */
    void store(const QString &requestId, const QString &requestName, const ResponseData &response);

    /** Forgets every response, for example when another workspace opens. */
    void clear();

    /** Returns whether no response is kept. */
    bool isEmpty() const;

    /**
     * Returns the value \a selector picks from the last response of \a request, a
     * request id or name, or nothing. The request \a exceptRequestId is never read.
     */
    std::optional<QString> value(const QString &request, const QString &selector,
                                 const QString &exceptRequestId) const;

    /** Returns the names of the requests with a kept response, newest first, without \a exceptRequestId. */
    QStringList requestNames(const QString &exceptRequestId) const;

private:
    /** One kept response. */
    struct Entry
    {
        /** Name of the request when it was sent. */
        QString name;
        /** Response headers as received. */
        QList<std::pair<QByteArray, QByteArray>> headers;
        /** The whole body; empty when it was too large for memory. */
        QByteArray body;
        /** Order of storing; higher is newer. */
        quint64 order {0};
    };

    /** Kept responses by request id. */
    QHash<QString, Entry> m_entries;

    /** Order given to the next stored response. */
    quint64 m_nextOrder {0};
};

#endif // RESPONSECACHE_H
