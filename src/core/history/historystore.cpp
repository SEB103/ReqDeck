// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/history/historystore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSet>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include "core/http/credentialnames.h"
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
 * \brief How long an add or clear waits for another instance to finish writing.
 */
constexpr int kLockTimeoutMs = 3000;

/*!
 * \internal
 * \brief Cuts \a text to HistoryStore::kMaxStoredValueChars, without splitting a surrogate pair.
 */
void capText(QString &text)
{
    if (text.size() <= HistoryStore::kMaxStoredValueChars)
        return;
    text.truncate(HistoryStore::kMaxStoredValueChars);
    if (text.back().isHighSurrogate())
        text.chop(1);
}

/*!
 * \internal
 * \brief Cuts the keys and values of \a rows.
 */
void capRows(QList<KeyValue> &rows)
{
    for (KeyValue &row : rows) {
        capText(row.key);
        capText(row.value);
    }
}

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
 * \brief Clears credentials in a fragment of \a url that holds "name=value" pairs.
 *
 * OAuth implicit flows return the token there, as in "#access_token=...".
 * A plain anchor such as "#section-2" has no pairs and stays.
 */
void redactFragment(QString &url)
{
    const qsizetype hash = url.indexOf(u'#');
    if (hash < 0 || url.indexOf(u'=', hash) < 0)
        return;
    url.replace(hash + 1, url.size() - hash - 1, redactedPairs(url.sliced(hash + 1)));
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
    m_entries.clear();
    const FileState state = readEntries(m_entries);
    m_unread = state == FileState::Unreadable;
    if (state == FileState::Unreadable)
        return fail(errorString, QStringLiteral("The history file could not be read."));
    if (state == FileState::SetAside)
        return fail(errorString, QStringLiteral("The history file is not valid."));
    return true;
}

/*!
 * \internal
 * \brief Reads the entries of the file into \a entries.
 *
 * A single invalid entry is skipped, so one damaged row does not cost the whole
 * history. A file that cannot be understood at all, including one written by a
 * newer schema, is renamed to "<file>.corrupt" and reads as empty. A file that
 * can neither be read nor moved is Unreadable, and nothing may overwrite it.
 * \param entries Receives the entries, newest first; unchanged unless Read.
 * \return The outcome.
 */
HistoryStore::FileState HistoryStore::readEntries(QList<HistoryEntry> &entries) const
{
    QFile file(m_filePath);
    if (!file.exists())
        return FileState::Read;
    if (!file.open(QIODevice::ReadOnly))
        return FileState::Unreadable;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    const QJsonObject root = document.object();
    const QJsonValue values = root.value(QLatin1StringView("entries"));
    if (!document.isObject() || root.value(QLatin1StringView("schema")).toInt() != kHistorySchemaVersion
        || !values.isArray()) {
        return setAsideCorruptFile(m_filePath) ? FileState::SetAside : FileState::Unreadable;
    }

    QList<HistoryEntry> read;
    for (const QJsonValue &value : values.toArray()) {
        if (std::optional<HistoryEntry> entry = entryFromJson(value))
            read.append(std::move(*entry));
        if (read.size() == kMaxEntries)
            break;
    }
    entries = std::move(read);
    return FileState::Read;
}

/*!
 * \brief Returns the entries, newest first.
 */
const QList<HistoryEntry> &HistoryStore::entries() const
{
    return m_entries;
}

/*!
 * \brief Writes \a entry as the newest one, in front of what the file holds now.
 *
 * A raw body above kMaxStoredBodyChars is left out and flagged with
 * HistoryEntry::bodyOmitted, and every other text is cut at
 * kMaxStoredValueChars, because the whole file is rewritten on every call.
 *
 * Two running instances share the file: the file is read again under a lock
 * file, so entries the other one wrote since are kept, and an entry it cleared
 * stays cleared. Entries of this store that could not be written before go in
 * too. While the file cannot be locked or read, nothing is written; the entry
 * waits for the next add.
 * \param entry The entry; an empty id and an invalid timestamp are filled in.
 * \return The entries the list becomes; Update::prependsOne tells a model whether
 *         one inserted row describes the change.
 */
HistoryStore::Update HistoryStore::prepareAdd(HistoryEntry entry)
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
    RequestSpec &request = entry.request;
    capText(request.url);
    capRows(request.queryParams);
    capRows(request.headers);
    capRows(request.body.form);
    for (QString *text : {&request.auth.username, &request.auth.password, &request.auth.token,
                          &request.auth.apiKeyName, &request.auth.apiKeyValue, &entry.errorString}) {
        capText(*text);
    }

    QList<HistoryEntry> expected = m_entries;
    expected.prepend(entry);
    while (expected.size() > kMaxEntries)
        expected.removeLast();

    Update update;
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QLockFile lock(m_filePath + QStringLiteral(".lock"));
    const bool locked = lock.tryLock(kLockTimeoutMs);
    QList<HistoryEntry> inFile;
    if (!locked || readEntries(inFile) == FileState::Unreadable) {
        m_pending.prepend(std::move(entry));
        m_unread = true;
        update.errorString = locked ? QStringLiteral("The history file could not be read.")
                                    : QStringLiteral("The history file is in use.");
        update.entries = std::move(expected);
        return update;
    }

    QList<HistoryEntry> merged;
    QSet<QString> ids;
    for (const QList<HistoryEntry> *part : {&m_pending, &inFile}) {
        for (const HistoryEntry &item : *part) {
            if (!ids.contains(item.id) && merged.size() < kMaxEntries - 1) {
                ids.insert(item.id);
                merged.append(item);
            }
        }
    }
    merged.prepend(entry);

    QJsonArray values;
    for (const HistoryEntry &item : std::as_const(merged))
        values.append(entryToJson(item));
    QJsonObject root;
    root.insert(QLatin1StringView("schema"), kHistorySchemaVersion);
    root.insert(QLatin1StringView("entries"), values);
    update.ok = writeJsonFile(m_filePath, root);
    if (update.ok) {
        m_pending.clear();
        m_unread = false;
    } else {
        m_pending.prepend(std::move(entry));
        update.errorString = QStringLiteral("The history file could not be written.");
    }
    update.prependsOne = merged == expected;
    update.entries = std::move(merged);
    return update;
}

/*!
 * \brief Makes the entries of \a update the list in memory.
 * \param update The result of prepareAdd().
 */
void HistoryStore::apply(Update update)
{
    m_entries = std::move(update.entries);
}

/*!
 * \brief Adds \a entry; see prepareAdd().
 * \param entry The entry.
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file could not be read or written; the entry stays in memory.
 */
bool HistoryStore::add(HistoryEntry entry, QString *errorString)
{
    Update update = prepareAdd(std::move(entry));
    const bool ok = update.ok;
    if (!ok && errorString)
        *errorString = update.errorString;
    apply(std::move(update));
    return ok;
}

/*!
 * \brief Removes all entries, the file, and a set-aside "<file>.corrupt".
 *
 * The lock keeps another instance from writing the old entries back in the
 * middle of it; that instance shows the empty history with its next add. When
 * the file cannot be locked or removed, nothing changes, also in memory.
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file could not be locked or removed.
 */
bool HistoryStore::clear(QString *errorString)
{
    QLockFile lock(m_filePath + QStringLiteral(".lock"));
    if (QFileInfo(m_filePath).absoluteDir().exists() && !lock.tryLock(kLockTimeoutMs))
        return fail(errorString, QStringLiteral("The history file is in use."));
    if (QFile::exists(m_filePath) && !QFile::remove(m_filePath))
        return fail(errorString, QStringLiteral("The history file could not be removed."));
    const QString corrupt = m_filePath + QStringLiteral(".corrupt");
    if (QFile::exists(corrupt))
        QFile::remove(corrupt);
    m_entries.clear();
    m_pending.clear();
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
    clearLiteral(result.auth.oauth2.clientSecret);

    // Basic without a password has the secret in the user name, as in curl -u token:
    if (spec.auth.type == AuthType::Basic && spec.auth.password.isEmpty())
        clearLiteral(result.auth.username);

    redactUserInfo(result.url);
    redactQuery(result.url);
    redactFragment(result.url);
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
