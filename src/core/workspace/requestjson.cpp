// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/requestjson.h"

#include <QJsonArray>
#include <QList>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace {
/*!
 * \internal
 * \brief Maps an enum value to the token that represents it in JSON.
 */
template<typename Enum>
using Token = std::pair<Enum, QLatin1StringView>;

/*!
 * \internal
 * \brief Tokens of BodyMode.
 */
constexpr std::array<Token<BodyMode>, 5> kBodyModeTokens {{
    {BodyMode::None, QLatin1StringView("none")},
    {BodyMode::Raw, QLatin1StringView("raw")},
    {BodyMode::FormUrlEncoded, QLatin1StringView("form")},
    {BodyMode::FormData, QLatin1StringView("formData")},
    {BodyMode::File, QLatin1StringView("file")},
}};

/*!
 * \internal
 * \brief Tokens of RawBodyType.
 */
constexpr std::array<Token<RawBodyType>, 3> kRawTypeTokens {{
    {RawBodyType::Json, QLatin1StringView("json")},
    {RawBodyType::Text, QLatin1StringView("text")},
    {RawBodyType::Xml, QLatin1StringView("xml")},
}};

/*!
 * \internal
 * \brief Tokens of AuthType.
 */
constexpr std::array<Token<AuthType>, 6> kAuthTypeTokens {{
    {AuthType::None, QLatin1StringView("none")},
    {AuthType::Basic, QLatin1StringView("basic")},
    {AuthType::Bearer, QLatin1StringView("bearer")},
    {AuthType::ApiKey, QLatin1StringView("apiKey")},
    {AuthType::Inherit, QLatin1StringView("inherit")},
    {AuthType::OAuth2, QLatin1StringView("oauth2")},
}};

/*!
 * \internal
 * \brief Tokens of OAuthGrant.
 */
constexpr std::array<Token<OAuthGrant>, 2> kGrantTokens {{
    {OAuthGrant::AuthorizationCode, QLatin1StringView("authorizationCode")},
    {OAuthGrant::ClientCredentials, QLatin1StringView("clientCredentials")},
}};

/*!
 * \internal
 * \brief Tokens of ApiKeyLocation.
 */
constexpr std::array<Token<ApiKeyLocation>, 2> kKeyLocationTokens {{
    {ApiKeyLocation::Header, QLatin1StringView("header")},
    {ApiKeyLocation::Query, QLatin1StringView("query")},
}};

/*!
 * \internal
 * \brief Returns the token of \a value in \a tokens.
 */
template<typename Enum, std::size_t Size>
QString tokenOf(Enum value, const std::array<Token<Enum>, Size> &tokens)
{
    for (const auto &[candidate, token] : tokens) {
        if (candidate == value)
            return token;
    }
    return {};
}

/*!
 * \internal
 * \brief Writes \a text under \a key unless it is empty.
 */
void insertText(QJsonObject &object, QLatin1StringView key, const QString &text)
{
    if (!text.isEmpty())
        object.insert(key, text);
}

/*!
 * \internal
 * \brief Writes \a rows under \a key unless the list is empty.
 */
void insertRows(QJsonObject &object, QLatin1StringView key, const QList<KeyValue> &rows)
{
    if (rows.isEmpty())
        return;
    QJsonArray array;
    for (const KeyValue &row : rows)
        array.append(keyValueToJson(row));
    object.insert(key, array);
}

/*!
 * \internal
 * \brief Reads the optional string member \a key into \a target.
 * \return False when the member exists and is not a string.
 */
bool readText(const QJsonObject &object, QLatin1StringView key, QString &target)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return true;
    if (!value.isString())
        return false;
    target = value.toString();
    return true;
}

/*!
 * \internal
 * \brief Reads the optional token member \a key into \a target.
 * \return False when the member exists and is not one of \a tokens.
 */
template<typename Enum, std::size_t Size>
bool readToken(const QJsonObject &object, QLatin1StringView key,
               const std::array<Token<Enum>, Size> &tokens, Enum &target)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return true;
    for (const auto &[candidate, token] : tokens) {
        if (value.isString() && value.toString() == token) {
            target = candidate;
            return true;
        }
    }
    return false;
}

/*!
 * \internal
 * \brief Reads the optional row array member \a key into \a target.
 * \return False when the member is not an array of valid rows.
 */
bool readRows(const QJsonObject &object, QLatin1StringView key, QList<KeyValue> &target)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return true;
    if (!value.isArray())
        return false;
    QList<KeyValue> rows;
    for (const QJsonValue &entry : value.toArray()) {
        const std::optional<KeyValue> row = keyValueFromJson(entry);
        if (!row)
            return false;
        rows.append(*row);
    }
    target = std::move(rows);
    return true;
}

/*!
 * \internal
 * \brief Reads the optional array of form parts \a key into \a target.
 * \return False when the member is not an array of valid parts.
 */
bool readParts(const QJsonObject &object, QLatin1StringView key, QList<FormPart> &target)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return true;
    if (!value.isArray())
        return false;
    QList<FormPart> parts;
    for (const QJsonValue &entry : value.toArray()) {
        const std::optional<FormPart> part = formPartFromJson(entry);
        if (!part)
            return false;
        parts.append(*part);
    }
    target = std::move(parts);
    return true;
}

/*!
 * \internal
 * \brief Reads the optional non-negative integer member \a key into \a target.
 * \return False when the member is anything else.
 */
bool readTimeout(const QJsonObject &object, QLatin1StringView key, int &target)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined())
        return true;
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (number < 0 || number > std::numeric_limits<int>::max() || number != std::floor(number))
        return false;
    target = int(number);
    return true;
}

/*!
 * \internal
 * \brief Returns the JSON form of \a body; empty when every member is at its default.
 */
QJsonObject bodyToJson(const RequestBody &body)
{
    QJsonObject object;
    if (body.mode != BodyMode::None)
        object.insert(QLatin1StringView("mode"), tokenOf(body.mode, kBodyModeTokens));
    if (body.rawType != RawBodyType::Json)
        object.insert(QLatin1StringView("rawType"), tokenOf(body.rawType, kRawTypeTokens));
    insertText(object, QLatin1StringView("raw"), body.raw);
    insertRows(object, QLatin1StringView("form"), body.form);
    if (!body.parts.isEmpty()) {
        QJsonArray parts;
        for (const FormPart &part : body.parts)
            parts.append(formPartToJson(part));
        object.insert(QLatin1StringView("parts"), parts);
    }
    insertText(object, QLatin1StringView("file"), body.filePath);
    return object;
}

/*!
 * \internal
 * \brief Returns the JSON form of \a auth; empty when every member is at its default.
 */
QJsonObject authObject(const RequestAuth &auth)
{
    QJsonObject object;
    if (auth.type != AuthType::None)
        object.insert(QLatin1StringView("type"), tokenOf(auth.type, kAuthTypeTokens));
    insertText(object, QLatin1StringView("username"), auth.username);
    insertText(object, QLatin1StringView("password"), auth.password);
    insertText(object, QLatin1StringView("token"), auth.token);
    insertText(object, QLatin1StringView("apiKeyName"), auth.apiKeyName);
    insertText(object, QLatin1StringView("apiKeyValue"), auth.apiKeyValue);
    if (auth.apiKeyLocation != ApiKeyLocation::Header) {
        object.insert(QLatin1StringView("apiKeyLocation"),
                      tokenOf(auth.apiKeyLocation, kKeyLocationTokens));
    }
    if (auth.oauth2 != OAuth2Settings()) {
        QJsonObject oauth;
        if (auth.oauth2.grant != OAuthGrant::AuthorizationCode)
            oauth.insert(QLatin1StringView("grant"), tokenOf(auth.oauth2.grant, kGrantTokens));
        insertText(oauth, QLatin1StringView("authUrl"), auth.oauth2.authUrl);
        insertText(oauth, QLatin1StringView("tokenUrl"), auth.oauth2.tokenUrl);
        insertText(oauth, QLatin1StringView("clientId"), auth.oauth2.clientId);
        insertText(oauth, QLatin1StringView("clientSecret"), auth.oauth2.clientSecret);
        insertText(oauth, QLatin1StringView("scope"), auth.oauth2.scope);
        insertText(oauth, QLatin1StringView("audience"), auth.oauth2.audience);
        object.insert(QLatin1StringView("oauth2"), oauth);
    }
    return object;
}

/*!
 * \internal
 * \brief Sets \a errorString, when not null, to the message for \a member.
 */
std::nullopt_t invalidMember(QString *errorString, const QString &member)
{
    if (errorString)
        *errorString = QStringLiteral("Invalid request member: %1").arg(member);
    return std::nullopt;
}
} // namespace

/*!
 * \brief Returns the JSON form of the form part \a part; defaults are left out.
 * \param part The part of a multipart/form-data body.
 */
QJsonObject formPartToJson(const FormPart &part)
{
    QJsonObject object;
    object.insert(QLatin1StringView("key"), part.key);
    object.insert(QLatin1StringView("value"), part.value);
    if (!part.enabled)
        object.insert(QLatin1StringView("enabled"), false);
    if (part.isFile)
        object.insert(QLatin1StringView("file"), true);
    if (!part.contentType.isEmpty())
        object.insert(QLatin1StringView("contentType"), part.contentType);
    return object;
}

/*!
 * \brief Parses one form part.
 * \param value An object with "key" and "value" strings and optional
 *        "enabled" and "file" booleans and a "contentType" string.
 * \return The part, or std::nullopt for anything else.
 */
std::optional<FormPart> formPartFromJson(const QJsonValue &value)
{
    if (!value.isObject())
        return std::nullopt;
    const QJsonObject object = value.toObject();
    const QJsonValue key = object.value(QLatin1StringView("key"));
    const QJsonValue text = object.value(QLatin1StringView("value"));
    const QJsonValue enabled = object.value(QLatin1StringView("enabled"));
    const QJsonValue file = object.value(QLatin1StringView("file"));
    const QJsonValue contentType = object.value(QLatin1StringView("contentType"));
    if (!key.isString() || !text.isString() || (!enabled.isUndefined() && !enabled.isBool())
        || (!file.isUndefined() && !file.isBool()) || (!contentType.isUndefined() && !contentType.isString()))
        return std::nullopt;
    FormPart part;
    part.key = key.toString();
    part.value = text.toString();
    part.enabled = enabled.toBool(true);
    part.isFile = file.toBool(false);
    part.contentType = contentType.toString();
    return part;
}

/*!
 * \brief Returns the JSON form of \a row.
 * \param row The key/value row.
 */
QJsonObject keyValueToJson(const KeyValue &row)
{
    QJsonObject object;
    object.insert(QLatin1StringView("key"), row.key);
    object.insert(QLatin1StringView("value"), row.value);
    if (!row.enabled)
        object.insert(QLatin1StringView("enabled"), false);
    return object;
}

/*!
 * \brief Parses one key/value row.
 *
 * Absent members default to an empty string and an enabled row.
 * \param value The JSON value of the row.
 * \return The row, or std::nullopt when \a value is not an object or a member
 *         has the wrong type.
 */
std::optional<KeyValue> keyValueFromJson(const QJsonValue &value)
{
    if (!value.isObject())
        return std::nullopt;
    const QJsonObject object = value.toObject();

    KeyValue row;
    if (!readText(object, QLatin1StringView("key"), row.key)
        || !readText(object, QLatin1StringView("value"), row.value)) {
        return std::nullopt;
    }
    const QJsonValue enabled = object.value(QLatin1StringView("enabled"));
    if (!enabled.isUndefined() && !enabled.isBool())
        return std::nullopt;
    row.enabled = enabled.toBool(true);
    return row;
}

/*!
 * \brief Returns the JSON form of \a request.
 *
 * "method" and "url" are always written; every other member only when it
 * differs from its default, which keeps workspace files small and their diffs
 * readable. Fields of an inactive body mode or auth type are kept, so switching
 * back restores what the user entered.
 * \param request The request to serialize.
 */
QJsonObject requestToJson(const RequestSpec &request)
{
    QJsonObject object;
    object.insert(QLatin1StringView("method"), QString::fromLatin1(httpMethodVerb(request.method)));
    object.insert(QLatin1StringView("url"), request.url);
    insertRows(object, QLatin1StringView("queryParams"), request.queryParams);
    insertRows(object, QLatin1StringView("headers"), request.headers);
    if (const QJsonObject body = bodyToJson(request.body); !body.isEmpty())
        object.insert(QLatin1StringView("body"), body);
    if (const QJsonObject auth = authObject(request.auth); !auth.isEmpty())
        object.insert(QLatin1StringView("auth"), auth);
    if (request.timeoutMs != 0)
        object.insert(QLatin1StringView("timeoutMs"), request.timeoutMs);
    return object;
}

/*!
 * \brief Parses a request from its JSON form.
 *
 * Absent members take their defaults and unknown members are ignored, so files
 * written by a newer build of the same schema still load.
 * \param object The JSON object of the request.
 * \param errorString Receives the reason on failure when it is not null.
 * \return The request, or std::nullopt for a member of the wrong type or with
 *         an unknown value.
 */
std::optional<RequestSpec> requestFromJson(const QJsonObject &object, QString *errorString)
{
    RequestSpec request;

    const QJsonValue method = object.value(QLatin1StringView("method"));
    if (!method.isUndefined()) {
        if (!method.isString())
            return invalidMember(errorString, QStringLiteral("method"));
        const std::optional<HttpMethod> parsed = httpMethodFromString(method.toString());
        if (!parsed) {
            if (errorString)
                *errorString = QStringLiteral("Invalid method: %1").arg(method.toString());
            return std::nullopt;
        }
        request.method = *parsed;
    }

    if (!readText(object, QLatin1StringView("url"), request.url))
        return invalidMember(errorString, QStringLiteral("url"));
    if (!readRows(object, QLatin1StringView("queryParams"), request.queryParams))
        return invalidMember(errorString, QStringLiteral("queryParams"));
    if (!readRows(object, QLatin1StringView("headers"), request.headers))
        return invalidMember(errorString, QStringLiteral("headers"));
    if (!readTimeout(object, QLatin1StringView("timeoutMs"), request.timeoutMs))
        return invalidMember(errorString, QStringLiteral("timeoutMs"));

    const QJsonValue bodyValue = object.value(QLatin1StringView("body"));
    if (!bodyValue.isUndefined() && !bodyValue.isObject())
        return invalidMember(errorString, QStringLiteral("body"));
    const QJsonObject body = bodyValue.toObject();
    if (!readToken(body, QLatin1StringView("mode"), kBodyModeTokens, request.body.mode))
        return invalidMember(errorString, QStringLiteral("body.mode"));
    if (!readToken(body, QLatin1StringView("rawType"), kRawTypeTokens, request.body.rawType))
        return invalidMember(errorString, QStringLiteral("body.rawType"));
    if (!readText(body, QLatin1StringView("raw"), request.body.raw))
        return invalidMember(errorString, QStringLiteral("body.raw"));
    if (!readRows(body, QLatin1StringView("form"), request.body.form))
        return invalidMember(errorString, QStringLiteral("body.form"));
    if (!readParts(body, QLatin1StringView("parts"), request.body.parts))
        return invalidMember(errorString, QStringLiteral("body.parts"));
    if (!readText(body, QLatin1StringView("file"), request.body.filePath))
        return invalidMember(errorString, QStringLiteral("body.file"));

    const QJsonValue authValue = object.value(QLatin1StringView("auth"));
    if (!authValue.isUndefined() && !authValue.isObject())
        return invalidMember(errorString, QStringLiteral("auth"));
    std::optional<RequestAuth> auth = authFromJson(authValue.toObject(), errorString);
    if (!auth)
        return std::nullopt;
    request.auth = std::move(*auth);

    return request;
}

/*!
 * \brief Returns the JSON form of \a auth; only non-default members are written.
 * \param auth The authentication settings of a request or folder.
 */
QJsonObject authToJson(const RequestAuth &auth)
{
    return authObject(auth);
}

/*!
 * \brief Parses authentication settings.
 *
 * Absent members take the defaults of RequestAuth; a member of the wrong type
 * or with an unknown value fails.
 * \param object The JSON object.
 * \param errorString Receives the reason on failure when it is not null.
 * \return The settings, or std::nullopt.
 */
std::optional<RequestAuth> authFromJson(const QJsonObject &object, QString *errorString)
{
    RequestAuth auth;
    if (!readToken(object, QLatin1StringView("type"), kAuthTypeTokens, auth.type))
        return invalidMember(errorString, QStringLiteral("auth.type"));
    if (!readToken(object, QLatin1StringView("apiKeyLocation"), kKeyLocationTokens, auth.apiKeyLocation))
        return invalidMember(errorString, QStringLiteral("auth.apiKeyLocation"));
    if (!readText(object, QLatin1StringView("username"), auth.username)
        || !readText(object, QLatin1StringView("password"), auth.password)
        || !readText(object, QLatin1StringView("token"), auth.token)
        || !readText(object, QLatin1StringView("apiKeyName"), auth.apiKeyName)
        || !readText(object, QLatin1StringView("apiKeyValue"), auth.apiKeyValue)) {
        return invalidMember(errorString, QStringLiteral("auth"));
    }
    const QJsonValue oauthValue = object.value(QLatin1StringView("oauth2"));
    if (!oauthValue.isUndefined() && !oauthValue.isObject())
        return invalidMember(errorString, QStringLiteral("auth.oauth2"));
    const QJsonObject oauth = oauthValue.toObject();
    if (!readToken(oauth, QLatin1StringView("grant"), kGrantTokens, auth.oauth2.grant)
        || !readText(oauth, QLatin1StringView("authUrl"), auth.oauth2.authUrl)
        || !readText(oauth, QLatin1StringView("tokenUrl"), auth.oauth2.tokenUrl)
        || !readText(oauth, QLatin1StringView("clientId"), auth.oauth2.clientId)
        || !readText(oauth, QLatin1StringView("clientSecret"), auth.oauth2.clientSecret)
        || !readText(oauth, QLatin1StringView("scope"), auth.oauth2.scope)
        || !readText(oauth, QLatin1StringView("audience"), auth.oauth2.audience)) {
        return invalidMember(errorString, QStringLiteral("auth.oauth2"));
    }
    return auth;
}
