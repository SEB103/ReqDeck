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
 * \brief Parts of header, parameter, and field names that mark a credential.
 */
constexpr std::array<QLatin1StringView, 17> kCredentialNameParts {
    QLatin1StringView("authorization"), QLatin1StringView("authentication"),
    QLatin1StringView("x-auth"),        QLatin1StringView("cookie"),
    QLatin1StringView("token"),         QLatin1StringView("secret"),
    QLatin1StringView("password"),      QLatin1StringView("passwd"),
    QLatin1StringView("pwd"),           QLatin1StringView("apikey"),
    QLatin1StringView("api-key"),       QLatin1StringView("api_key"),
    QLatin1StringView("access_key"),    QLatin1StringView("accesskey"),
    QLatin1StringView("session"),       QLatin1StringView("signature"),
    QLatin1StringView("jwt"),
};

/*!
 * \internal
 * \brief Short names that mark a credential only as a whole name: "key", not "monkey".
 */
constexpr std::array<QLatin1StringView, 4> kCredentialNames {
    QLatin1StringView("key"),
    QLatin1StringView("auth"),
    QLatin1StringView("pass"),
    QLatin1StringView("sig"),
};

/*!
 * \internal
 * \brief Authentication schemes that may stand before a reference in a value.
 */
constexpr std::array<QLatin1StringView, 3> kAuthSchemes {
    QLatin1StringView("Bearer"), QLatin1StringView("Basic"), QLatin1StringView("Token"),
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
 * \brief Clears \a value unless it holds nothing literal.
 *
 * What may stay is one or more references, optionally after an authentication
 * scheme, as in "Bearer {{token}}". A value with a literal part next to a
 * reference is cleared whole, because the literal part may be the secret.
 */
void clearLiteral(QString &value)
{
    const QList<VariableReference> references = VariableResolver::references(value);
    if (references.isEmpty()) {
        value.clear();
        return;
    }

    QString literal = value;
    for (auto it = references.crbegin(); it != references.crend(); ++it)
        literal.remove(it->start, it->length);
    literal = literal.trimmed();

    const bool scheme = std::any_of(kAuthSchemes.cbegin(), kAuthSchemes.cend(),
                                    [&literal](QLatin1StringView candidate) {
                                        return literal.compare(candidate, Qt::CaseInsensitive) == 0;
                                    });
    if (!literal.isEmpty() && !scheme)
        value.clear();
}

/*!
 * \internal
 * \brief Returns whether \a name marks its value as a credential.
 */
bool isCredentialName(QStringView name)
{
    const QStringView trimmed = name.trimmed();
    return std::any_of(kCredentialNames.cbegin(), kCredentialNames.cend(),
                       [trimmed](QLatin1StringView candidate) {
                           return trimmed.compare(candidate, Qt::CaseInsensitive) == 0;
                       })
        || std::any_of(kCredentialNameParts.cbegin(), kCredentialNameParts.cend(),
                       [trimmed](QLatin1StringView part) {
                           return trimmed.contains(part, Qt::CaseInsensitive);
                       });
}

/*!
 * \internal
 * \brief Returns whether \a value has the form of a credential, as in "Bearer abc".
 */
bool startsWithAuthScheme(QStringView value)
{
    return std::any_of(kAuthSchemes.cbegin(), kAuthSchemes.cend(),
                       [value](QLatin1StringView scheme) {
                           return value.size() > scheme.size() + 1
                               && value.startsWith(scheme, Qt::CaseInsensitive)
                               && value.at(scheme.size()).isSpace();
                       });
}

/*!
 * \internal
 * \brief Clears the literal values of the rows of \a rows named like credentials.
 */
void clearCredentialRows(QList<KeyValue> &rows)
{
    for (KeyValue &row : rows) {
        if (isCredentialName(row.key))
            clearLiteral(row.value);
    }
}

/*!
 * \internal
 * \brief Removes literal credentials from the user information of \a url.
 *
 * User information without a reference goes whole: a token often stands in the
 * place of the user name. With a reference, a literal password goes, and so does
 * a user name mixing a reference with a literal part. A URL without a scheme counts
 * too, because RequestBuilder prepends one and sends the credentials.
 */
void redactUserInfo(QString &url)
{
    const qsizetype scheme = url.indexOf(QLatin1StringView("://"));
    const qsizetype start = scheme < 0 ? 0 : scheme + 3;
    qsizetype end = start;
    while (end < url.size() && url.at(end) != u'/' && url.at(end) != u'?' && url.at(end) != u'#')
        ++end;
    const qsizetype at = url.first(end).lastIndexOf(u'@');
    if (at < start)
        return;

    QString info = url.sliced(start, at - start);
    const qsizetype colon = info.indexOf(u':');
    if (colon < 0)
        clearLiteral(info);
    if (info.isEmpty() || VariableResolver::references(info).isEmpty()) {
        url.remove(start, at - start + 1);
        return;
    }
    if (colon < 0)
        return;

    QString password = info.sliced(colon + 1);
    clearLiteral(password);
    url.replace(start, at - start, info.first(colon + 1) + password);
}

/*!
 * \internal
 * \brief Returns "a=b&c=d" text \a pairs with the literal credentials cleared.
 */
QString redactedPairs(const QString &pairs)
{
    QStringList parts = pairs.split(u'&');
    for (QString &part : parts) {
        const qsizetype equals = part.indexOf(u'=');
        if (equals < 0 || !isCredentialName(QStringView(part).first(equals)))
            continue;
        QString value = part.sliced(equals + 1);
        clearLiteral(value);
        part = part.first(equals + 1) + value;
    }
    return parts.join(u'&');
}

/*!
 * \internal
 * \brief Clears the literal values of query parameters of \a url named like credentials.
 */
void redactQuery(QString &url)
{
    const qsizetype query = url.indexOf(u'?');
    if (query < 0)
        return;

    qsizetype end = url.indexOf(u'#', query);
    if (end < 0)
        end = url.size();

    url.replace(query + 1, end - query - 1, redactedPairs(url.sliced(query + 1, end - query - 1)));
}

/*!
 * \internal
 * \brief Returns JSON \a text with the values of keys named like credentials cleared.
 *
 * The text is edited in place instead of being parsed and written again, which
 * would change the order of the keys and the formatting. String and number values
 * are covered at any depth; a number becomes an empty string. Possessive
 * quantifiers keep the pattern linear on long strings.
 */
QString redactedJson(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral(
        R"re("((?:[^"\\]++|\\.)*+)"\s*+:\s*+("(?:[^"\\]++|\\.)*+"|-?\d[\d.eE+\-]*+))re"));

    QString result;
    qsizetype position = 0;
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (!isCredentialName(match.capturedView(1)))
            continue;

        const qsizetype start = match.capturedStart(2);
        result += QStringView(text).sliced(position, start - position);
        position = match.capturedEnd(2);

        const QString value = match.captured(2);
        QString inner = value.startsWith(u'"') ? value.sliced(1, value.size() - 2) : QString();
        clearLiteral(inner);
        result += u'"' + inner + u'"';
    }
    result += QStringView(text).sliced(position);
    return result;
}

/*!
 * \internal
 * \brief Clears literal credentials from a raw body, judged by its first character.
 *
 * JSON and "a=b&c=d" text are covered. XML and other text stay as they are.
 */
void redactRawBody(QString &raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(u'<'))
        return;
    if (trimmed.startsWith(u'{') || trimmed.startsWith(u'['))
        raw = redactedJson(raw);
    else
        raw = redactedPairs(raw);
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
    const QJsonValue requestValue = object.value(QLatin1StringView("request"));
    if (!requestValue.isObject())
        return std::nullopt;
    const std::optional<RequestSpec> request = requestFromJson(requestValue.toObject());
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
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file is unreadable or unusable; the history is then empty.
 */
bool HistoryStore::load(QString *errorString)
{
    return readFile(errorString);
}

/*!
 * \internal
 * \brief Replaces the entries with the content of the file.
 *
 * A single invalid entry is skipped, so one damaged row does not cost the whole
 * history. A file that cannot be understood at all, including one written by a
 * newer schema, is renamed to "<file>.corrupt". As long as a file can neither be
 * read nor moved, m_unread stays set and add() does not overwrite it.
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file is unreadable or unusable.
 */
bool HistoryStore::readFile(QString *errorString)
{
    m_entries.clear();
    m_unread = true;

    QFile file(m_filePath);
    if (!file.exists()) {
        m_unread = false;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly))
        return fail(errorString, QStringLiteral("The history file could not be read."));

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    const QJsonObject root = document.object();
    const QJsonValue entries = root.value(QLatin1StringView("entries"));
    if (!document.isObject() || root.value(QLatin1StringView("schema")).toInt() != kHistorySchemaVersion
        || !entries.isArray()) {
        m_unread = !setAsideCorruptFile(m_filePath);
        return fail(errorString, QStringLiteral("The history file is not valid."));
    }

    for (const QJsonValue &value : entries.toArray()) {
        if (std::optional<HistoryEntry> entry = entryFromJson(value))
            m_entries.append(std::move(*entry));
        if (m_entries.size() == kMaxEntries)
            break;
    }
    m_unread = false;
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
 *
 * The file is replaced as a whole, so entries it may still hold are read first,
 * also when load() was never called. If that stays impossible nothing is
 * written; the entries in memory are merged with the file by a later call.
 * \param entry The entry; an empty id and an invalid timestamp are filled in.
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file could not be read or written; the entry stays in memory.
 */
bool HistoryStore::add(HistoryEntry entry, QString *errorString)
{
    if (entry.id.isEmpty())
        entry.id = newItemId();
    if (!entry.timestamp.isValid())
        entry.timestamp = QDateTime::currentDateTimeUtc();

    if (entry.request.body.raw.size() > kMaxStoredBodyChars) {
        entry.request.body.raw.clear();
        entry.bodyOmitted = true;
    }
    entry.request = redacted(entry.request);

    if (m_unread) {
        const QList<HistoryEntry> pending = m_entries;
        readFile(nullptr);
        m_entries = pending + m_entries;
    }

    m_entries.prepend(std::move(entry));
    while (m_entries.size() > kMaxEntries)
        m_entries.removeLast();

    if (m_unread)
        return fail(errorString, QStringLiteral("The history file could not be read."));

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
    m_unread = false;
    return true;
}

/*!
 * \brief Returns whether the file may hold entries that add() will merge into the list.
 */
bool HistoryStore::hasUnreadFile() const
{
    return m_unread;
}

/*!
 * \brief Removes the oldest entry from the list.
 *
 * This lets a model report the removal before add() reports the new row. The file
 * changes with the next add().
 */
void HistoryStore::dropOldest()
{
    if (!m_entries.isEmpty())
        m_entries.removeLast();
}

/*!
 * \brief Returns \a spec with literal credentials cleared.
 *
 * The history is a plain file next to the application data, so passwords,
 * tokens, API keys, credentials in the URL, and the values of headers, parameters,
 * form fields, and JSON or "a=b&c=d" raw bodies named like credentials are removed
 * unless they only reference variables: a reference discloses nothing and still
 * works when the request is repeated. Fields of an inactive auth type are cleared
 * as well. Names are a heuristic, and XML or other raw text stays as it is, because
 * a body is what makes a request worth repeating.
 * \param spec The request as edited by the user.
 */
RequestSpec HistoryStore::redacted(const RequestSpec &spec)
{
    RequestSpec result = spec;
    clearLiteral(result.auth.password);
    clearLiteral(result.auth.token);
    clearLiteral(result.auth.apiKeyValue);

    // Basic without a password has the secret in the user name, as in curl -u token:
    if (spec.auth.type == AuthType::Basic && spec.auth.password.isEmpty())
        clearLiteral(result.auth.username);

    redactUserInfo(result.url);
    redactQuery(result.url);
    clearCredentialRows(result.queryParams);
    clearCredentialRows(result.headers);
    clearCredentialRows(result.body.form);
    redactRawBody(result.body.raw);

    for (KeyValue &header : result.headers) {
        if (startsWithAuthScheme(header.value))
            clearLiteral(header.value);
    }

    const QString apiKeyName = spec.auth.apiKeyName.trimmed();
    if (!apiKeyName.isEmpty()) {
        const auto clearApiKey = [&apiKeyName](QList<KeyValue> &rows) {
            for (KeyValue &row : rows) {
                if (row.key.trimmed().compare(apiKeyName, Qt::CaseInsensitive) == 0)
                    clearLiteral(row.value);
            }
        };
        clearApiKey(result.headers);
        clearApiKey(result.queryParams);
    }
    return result;
}
