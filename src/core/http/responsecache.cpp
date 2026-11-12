// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/responsecache.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

#include "core/http/jsonpath.h"
#include "core/http/responsedata.h"

/*!
 * \class ResponseCache
 * \inmodule ReqDeckCore
 * \brief Keeps the last response of each request in memory for response() templates.
 */

/*!
 * \brief Keeps \a response as the last one of the request \a requestId.
 *
 * Only a response that arrived is kept: a transport failure leaves the earlier
 * response in place, so one failed send does not break the requests that read
 * it. Only a JSON body of at most kMaxJsonBytes in memory is kept, parsed, so
 * a template does not parse it again and large downloads are not held.
 * \param requestId Id of the request that was sent.
 * \param response The response.
 */
void ResponseCache::store(const QString &requestId, const ResponseData &response)
{
    if (requestId.isEmpty() || response.error != ResponseError::None || !response.hasStatus())
        return;
    Entry entry;
    entry.headers = response.headers;
    if (!response.bodyFile && !response.body.isEmpty() && response.body.size() <= kMaxJsonBytes) {
        const QByteArray trimmed = response.body.trimmed();
        if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
            const QJsonDocument document = QJsonDocument::fromJson(trimmed);
            if (document.isArray())
                entry.json = document.array();
            else if (document.isObject())
                entry.json = document.object();
        }
    }
    entry.order = ++m_nextOrder;
    m_entries.insert(requestId, std::move(entry));
}

/*!
 * \brief Forgets every response.
 */
void ResponseCache::clear()
{
    m_entries.clear();
}

/*!
 * \brief Returns whether no response is kept.
 */
bool ResponseCache::isEmpty() const
{
    return m_entries.isEmpty();
}

/*!
 * \brief Returns whether a response of the request \a requestId is kept.
 */
bool ResponseCache::contains(const QString &requestId) const
{
    return m_entries.contains(requestId);
}

/*!
 * \brief Returns the value \a selector picks from the newest kept response of \a requestIds.
 *
 * The caller maps a request name to the ids that have it now, so a renamed
 * request is found by its new name. \a selector is either "header:Name", the
 * first header of that name, or a JSONPath into a JSON body, whose first match
 * is returned (object members come sorted by name, as Qt keeps them): a string
 * as it is, any other value as compact JSON. A missing response, header, or
 * match gives nothing.
 * \param requestIds Ids of the requests whose newest response is read.
 * \param selector What to read from it.
 */
std::optional<QString> ResponseCache::value(const QStringList &requestIds, const QString &selector) const
{
    const Entry *entry = nullptr;
    for (const QString &id : requestIds) {
        const auto it = m_entries.constFind(id);
        if (it != m_entries.cend() && (!entry || it->order > entry->order))
            entry = &it.value();
    }
    if (!entry)
        return std::nullopt;

    const QString trimmed = selector.trimmed();
    const QLatin1StringView headerPrefix("header:");
    if (trimmed.startsWith(headerPrefix, Qt::CaseInsensitive)) {
        const QByteArray name = trimmed.sliced(headerPrefix.size()).trimmed().toLatin1();
        if (name.isEmpty())
            return std::nullopt;
        for (const auto &[headerName, value] : entry->headers) {
            if (headerName.compare(name, Qt::CaseInsensitive) == 0)
                return QString::fromUtf8(value);
        }
        return std::nullopt;
    }

    if (entry->json.isUndefined())
        return std::nullopt;
    const JsonPathResult result = JsonPath::evaluate(entry->json, trimmed);
    if (!result.ok || result.matches.isEmpty())
        return std::nullopt;
    const QJsonValue match = result.matches.first();
    switch (match.type()) {
    case QJsonValue::String:
        return match.toString();
    case QJsonValue::Object:
        return QString::fromUtf8(QJsonDocument(match.toObject()).toJson(QJsonDocument::Compact));
    case QJsonValue::Array:
        return QString::fromUtf8(QJsonDocument(match.toArray()).toJson(QJsonDocument::Compact));
    default: {
        // Numbers, true, false, and null as JSON writes them.
        const QByteArray wrapped = QJsonDocument(QJsonArray({match})).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(wrapped.sliced(1, wrapped.size() - 2));
    }
    }
}

/*!
 * \brief Returns the ids of the requests with a kept response, newest first.
 */
QStringList ResponseCache::requestIds() const
{
    QList<std::pair<quint64, QString>> ordered;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it)
        ordered.append({it->order, it.key()});
    std::sort(ordered.begin(), ordered.end(), [](const auto &lhs, const auto &rhs) { return lhs.first > rhs.first; });
    QStringList ids;
    for (const auto &[order, id] : ordered)
        ids.append(id);
    return ids;
}

/*!
 * \brief Forgets the responses of requests not in \a requestIds.
 * \param requestIds Ids of the requests that still exist.
 */
void ResponseCache::retain(const QSet<QString> &requestIds)
{
    m_entries.removeIf([&requestIds](const auto &entry) { return !requestIds.contains(entry.key()); });
}
