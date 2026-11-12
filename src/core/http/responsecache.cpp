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
 * \param requestName Name of the request when it was sent.
 * \param response The response.
 */
void ResponseCache::store(const QString &requestId, const QString &requestName, const ResponseData &response)
{
    if (requestId.isEmpty() || response.error != ResponseError::None || !response.hasStatus())
        return;
    Entry entry;
    entry.name = requestName;
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
 * \brief Returns the value \a selector picks from the last response of \a request.
 *
 * \a request is a request id or, failing that, a request name; when several
 * requests have that name, the one sent last counts, other than \a exceptRequestId. \a selector is either
 * "header:Name", the first header of that name, or a JSONPath into a JSON body,
 * whose first match is returned (object members come sorted by name, as Qt keeps
 * them): a string as it is, any other value as compact
 * JSON. A missing response, header, or match gives nothing, and so does the
 * request \a exceptRequestId, so a request never reads its own response.
 * \param request Id or name of the request whose response is read.
 * \param selector What to read from it.
 * \param exceptRequestId Id of the request that is being sent.
 */
std::optional<QString> ResponseCache::value(const QString &request, const QString &selector,
                                            const QString &exceptRequestId) const
{
    const Entry *entry = nullptr;
    QString entryId;
    if (const auto byId = m_entries.constFind(request); byId != m_entries.cend()) {
        entry = &byId.value();
        entryId = byId.key();
    } else {
        for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
            if (it.key() != exceptRequestId && it->name == request && (!entry || it->order > entry->order)) {
                entry = &it.value();
                entryId = it.key();
            }
        }
    }
    if (!entry || entryId == exceptRequestId)
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
 * \brief Returns the names of the requests with a kept response, newest first.
 *
 * Each name appears once; the request \a exceptRequestId is left out, as a
 * request cannot read its own response.
 * \param exceptRequestId Id of the request that is open.
 */
QStringList ResponseCache::requestNames(const QString &exceptRequestId) const
{
    QList<std::pair<quint64, QString>> ordered;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        if (it.key() != exceptRequestId && !it->name.isEmpty())
            ordered.append({it->order, it->name});
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto &lhs, const auto &rhs) { return lhs.first > rhs.first; });
    QStringList names;
    for (const auto &[order, name] : ordered) {
        if (!names.contains(name))
            names.append(name);
    }
    return names;
}
