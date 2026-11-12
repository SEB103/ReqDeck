// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REQUESTSPEC_H
#define REQUESTSPEC_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringView>

#include <optional>

/** HTTP methods supported by ReqDeck requests. */
enum class HttpMethod {
    Get,
    Post,
    Put,
    Patch,
    Delete,
    Head,
    Options
};

/** Returns the upper-case request verb of \a method, for example "GET". */
QByteArray httpMethodVerb(HttpMethod method);

/** Parses a method name, ignoring case and surrounding whitespace; nullopt when unknown. */
std::optional<HttpMethod> httpMethodFromString(QStringView text);

/** One key/value row of a parameter, header, form, or variable table. */
struct KeyValue
{
    /** Row key, for example a header name. */
    QString key;
    /** Row value; may contain {{variable}} references. */
    QString value;
    /** Whether the row takes part in the request. */
    bool enabled {true};

    bool operator==(const KeyValue &) const = default;
};

/** How the request body is composed. */
enum class BodyMode {
    /** No body. */
    None,
    /** Free text sent as typed (see RawBodyType). */
    Raw,
    /** Key/value fields encoded as application/x-www-form-urlencoded. */
    FormUrlEncoded,
    /** Parts sent as multipart/form-data; a part is text or a file. */
    FormData,
    /** One file sent as the body, as it is on disk. */
    File
};

/** Content flavor of a raw body; selects the default Content-Type. */
enum class RawBodyType {
    Json,
    Text,
    Xml
};

/** One part of a multipart/form-data body. */
struct FormPart
{
    /** Field name. */
    QString key;
    /** Text of the field, or the path of the file; may contain {{variable}} references. */
    QString value;
    /** Whether the part is sent. */
    bool enabled {true};
    /** Whether value is the path of a file to send. */
    bool isFile {false};
    /** Content-Type of the part; empty picks one from the file name, or none for text. */
    QString contentType;

    bool operator==(const FormPart &) const = default;
};

/** Request body definition. */
struct RequestBody
{
    /** Body composition mode. */
    BodyMode mode {BodyMode::None};
    /** Flavor of the raw text; used only in BodyMode::Raw. */
    RawBodyType rawType {RawBodyType::Json};
    /** Raw body text; used only in BodyMode::Raw. */
    QString raw;
    /** Form fields; used only in BodyMode::FormUrlEncoded. */
    QList<KeyValue> form;
    /** Parts; used only in BodyMode::FormData. */
    QList<FormPart> parts;
    /** Path of the file sent as the body; used only in BodyMode::File; may contain {{variable}} references. */
    QString filePath;

    bool operator==(const RequestBody &) const = default;
};

/** Authentication scheme applied to a request. */
enum class AuthType {
    None,
    Basic,
    Bearer,
    ApiKey,
    /** Takes the auth of the nearest folder above that sets one. */
    Inherit
};

/** Where an API key is sent. */
enum class ApiKeyLocation {
    Header,
    Query
};

/** Authentication settings of a request. */
struct RequestAuth
{
    /** Selected scheme; other members are used only by the matching scheme. */
    AuthType type {AuthType::None};
    /** Basic auth user name. */
    QString username;
    /** Basic auth password. */
    QString password;
    /** Bearer token. */
    QString token;
    /** API key header or query parameter name. */
    QString apiKeyName;
    /** API key value. */
    QString apiKeyValue;
    /** Whether the API key goes into a header or the query string. */
    ApiKeyLocation apiKeyLocation {ApiKeyLocation::Header};

    bool operator==(const RequestAuth &) const = default;
};

/** Complete, unresolved definition of one HTTP request as the user edits it. */
struct RequestSpec
{
    /** Request method. */
    HttpMethod method {HttpMethod::Get};
    /** Request URL; may omit the scheme and contain {{variable}} references. */
    QString url;
    /** Query parameters appended to the URL. */
    QList<KeyValue> queryParams;
    /** Request headers; repeated names are allowed. */
    QList<KeyValue> headers;
    /** Request body. */
    RequestBody body;
    /** Authentication settings. */
    RequestAuth auth;
    /** Transfer timeout in milliseconds; 0 uses the builder default. */
    int timeoutMs {0};

    bool operator==(const RequestSpec &) const = default;
};

#endif // REQUESTSPEC_H
