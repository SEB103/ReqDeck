// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/history/historystore.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include "core/http/variableresolver.h"
#include "core/workspace/jsonfile.h"
#include "core/workspace/requestjson.h"
#include "core/workspace/workspacedata.h"

namespace {
/*!
 * \internal
 * \brief Schema version of the history file.
 */
constexpr int kHistorySchemaVersion = 1;

/*!
 * \internal
 * \brief Header names whose values are treated as credentials.
 */
constexpr std::array<QLatin1StringView, 5> kCredentialHeaders {
    QLatin1StringView("Authorization"), QLatin1StringView("Proxy-Authorization"),
    QLatin1StringView("Cookie"),        QLatin1StringView("X-Api-Key"),
    QLatin1StringView("X-Auth-Token"),
};

/*!
 * \internal
 * \brief Sets \a errorString to \a message when it is not null and returns false.
 */
bool fail(QString *errorString, const QString &message)
{
    if (errorString)
        *errorString = message;
    return false;
}

/*!
 * \internal
 * \brief Clears \a value unless it references a variable.
 */
void clearLiteral(QString &value)
{
    if (VariableResolver::referencedNames(value).isEmpty())
        value.clear();
}

/*!
 * \internal
 * \brief Returns the JSON form of \a entry.
 */
QJsonObject entryToJson(const HistoryEntry &entry)
{
    QJsonObject object;
    object.insert(QLatin1StringView("id"), entry.id);
    object.insert(QLatin1StringView("timestamp"), entry.timestamp.toUTC().toString(Qt::ISODateWithMs));
    object.insert(QLatin1StringView("request"), requestToJson(entry.request));
    object.insert(QLatin1StringView("statusCode"), entry.statusCode);
    object.insert(QLatin1StringView("elapsedMs"), entry.elapsedMs);
    if (!entry.errorString.isEmpty())
        object.insert(QLatin1StringView("errorString"), entry.errorString);
    if (entry.bodyOmitted)
        object.insert(QLatin1StringView("bodyOmitted"), true);
    return object;
}

/*!
 * \internal
 * \brief Parses one entry; nullopt when \a value is not a usable entry.
 */
std::optional<HistoryEntry> entryFromJson(const QJsonValue &value)
{
    if (!value.isObject())
        return std::nullopt;
    const QJsonObject object = value.toObject();

    HistoryEntry entry;
    entry.id = object.value(QLatin1StringView("id")).toString();
    entry.timestamp = QDateTime::fromString(object.value(QLatin1StringView("timestamp")).toString(),
                                            Qt::ISODateWithMs);
    const std::optional<RequestSpec> request =
        requestFromJson(object.value(QLatin1StringView("request")).toObject());
    if (entry.id.isEmpty() || !entry.timestamp.isValid() || !request)
        return std::nullopt;

    entry.request = *request;
    entry.statusCode = object.value(QLatin1StringView("statusCode")).toInt();
    entry.elapsedMs = object.value(QLatin1StringView("elapsedMs")).toInteger();
    entry.errorString = object.value(QLatin1StringView("errorString")).toString();
    entry.bodyOmitted = object.value(QLatin1StringView("bodyOmitted")).toBool();
    return entry;
}
} // namespace

/*!
 * \brief Creates an empty store writing to \a filePath.
 * \param filePath The history file.
 */
HistoryStore::HistoryStore(QString filePath)
    : m_filePath(std::move(filePath))
{
}

/*!
 * \brief Reads the history file.
 *
 * A single invalid entry is skipped, so one damaged row does not cost the whole
 * history. A file that cannot be understood at all, including one written by a
 * newer schema, is renamed to "<file>.corrupt" rather than overwritten by the
 * next add().
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file is unreadable or unusable; the history is then empty.
 */
bool HistoryStore::load(QString *errorString)
{
    m_entries.clear();

    QFile file(m_filePath);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return fail(errorString, QStringLiteral("The history file could not be read."));

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    const QJsonObject root = document.object();
    const QJsonValue entries = root.value(QLatin1StringView("entries"));
    if (!document.isObject() || root.value(QLatin1StringView("schema")).toInt() != kHistorySchemaVersion
        || !entries.isArray()) {
        setAsideCorruptFile(m_filePath);
        return fail(errorString, QStringLiteral("The history file is not valid."));
    }

    for (const QJsonValue &value : entries.toArray()) {
        if (std::optional<HistoryEntry> entry = entryFromJson(value))
            m_entries.append(std::move(*entry));
        if (m_entries.size() == kMaxEntries)
            break;
    }
    return true;
}

/*!
 * \brief Returns the entries, newest first.
 */
const QList<HistoryEntry> &HistoryStore::entries() const
{
    return m_entries;
}

/*!
 * \brief Adds \a entry as the newest one and writes the file.
 *
 * A raw body above kMaxStoredBodyChars is left out and flagged with
 * HistoryEntry::bodyOmitted, because the whole file is rewritten on every call.
 * \param entry The entry; an empty id and an invalid timestamp are filled in.
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file could not be written; the entry stays in memory.
 */
bool HistoryStore::add(HistoryEntry entry, QString *errorString)
{
    if (entry.id.isEmpty())
        entry.id = newItemId();
    if (!entry.timestamp.isValid())
        entry.timestamp = QDateTime::currentDateTimeUtc();

    entry.request = redacted(entry.request);
    if (entry.request.body.raw.size() > kMaxStoredBodyChars) {
        entry.request.body.raw.clear();
        entry.bodyOmitted = true;
    }

    m_entries.prepend(std::move(entry));
    while (m_entries.size() > kMaxEntries)
        m_entries.removeLast();

    QJsonArray entries;
    for (const HistoryEntry &item : std::as_const(m_entries))
        entries.append(entryToJson(item));

    QJsonObject root;
    root.insert(QLatin1StringView("schema"), kHistorySchemaVersion);
    root.insert(QLatin1StringView("entries"), entries);
    if (!writeJsonFile(m_filePath, root))
        return fail(errorString, QStringLiteral("The history file could not be written."));
    return true;
}

/*!
 * \brief Removes all entries and the file.
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file could not be removed.
 */
bool HistoryStore::clear(QString *errorString)
{
    m_entries.clear();
    if (QFile::exists(m_filePath) && !QFile::remove(m_filePath))
        return fail(errorString, QStringLiteral("The history file could not be removed."));
    return true;
}

/*!
 * \brief Returns \a spec with literal credentials cleared.
 *
 * The history is a plain file next to the application data, so passwords,
 * tokens, API keys, and credential header values are removed unless they
 * reference a variable: a reference discloses nothing and still works when the
 * request is repeated. Fields of an inactive auth type are cleared as well.
 * \param spec The request as edited by the user.
 */
RequestSpec HistoryStore::redacted(const RequestSpec &spec)
{
    RequestSpec result = spec;
    clearLiteral(result.auth.password);
    clearLiteral(result.auth.token);
    clearLiteral(result.auth.apiKeyValue);

    const QString apiKeyName = spec.auth.apiKeyName.trimmed();
    for (KeyValue &header : result.headers) {
        const QString name = header.key.trimmed();
        const bool credential =
            std::any_of(kCredentialHeaders.cbegin(), kCredentialHeaders.cend(),
                        [&name](QLatin1StringView candidate) {
                            return name.compare(candidate, Qt::CaseInsensitive) == 0;
                        })
            || (!apiKeyName.isEmpty() && name.compare(apiKeyName, Qt::CaseInsensitive) == 0);
        if (credential)
            clearLiteral(header.value);
    }
    return result;
}
