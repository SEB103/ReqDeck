// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/requestbuilder.h"

#include <QHttpHeaders>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>
#include <chrono>
#include <utility>

#include "core/http/httpencoding.h"

namespace {
/*!
 * \internal
 * \brief Returns the separator used to merge repeated values of header \a name.
 *
 * RFC 9110 section 5.3 combines repeated fields with ", "; the Cookie header
 * uses "; " instead (RFC 6265 section 5.4).
 */
QString headerValueSeparator(const QString &name)
{
    return name.compare(QLatin1StringView("Cookie"), Qt::CaseInsensitive) == 0
               ? QStringLiteral("; ")
               : QStringLiteral(", ");
}

/*!
 * \internal
 * \brief Returns whether \a name is an RFC 9110 token, the syntax of a header name.
 */
bool isValidHeaderName(QStringView name)
{
    static const QLatin1StringView specials("!#$%&'*+-.^_`|~");
    if (name.isEmpty())
        return false;
    return std::all_of(name.begin(), name.end(), [](QChar c) {
        return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9')
               || specials.contains(c);
    });
}

/*!
 * \internal
 * \brief Returns whether \a value contains no control character other than tab.
 *
 * CR and LF in particular would allow header injection.
 */
bool isValidHeaderValue(QStringView value)
{
    return std::none_of(value.begin(), value.end(), [](QChar c) {
        return (c.unicode() < 0x20 && c != u'\t') || c.unicode() == 0x7F;
    });
}

/*!
 * \internal
 * \brief Appends header \a name with \a value to \a headers after validating both.
 *
 * The value is sent as UTF-8 bytes. QHttpHeaders accepts only Latin-1 text,
 * so the UTF-8 bytes are handed over as a Latin-1 view, which stores each
 * byte unchanged (bytes 0x80-0xFF are obs-text in RFC 9110).
 * \return An empty string on success, otherwise the error message.
 */
QString appendHeader(QHttpHeaders &headers, const QString &name, const QString &value)
{
    if (!isValidHeaderName(name))
        return QStringLiteral("Invalid header name: %1").arg(name);
    const QByteArray bytes = value.toUtf8();
    if (!isValidHeaderValue(value) || !headers.append(name, QLatin1StringView(bytes)))
        return QStringLiteral("Invalid value for header: %1").arg(name);
    return {};
}

/*!
 * \internal
 * \brief Returns whether \a url starts with a URI scheme followed by "://".
 */
bool hasScheme(const QString &url)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9+.\-]*://)"));
    return pattern.match(url).hasMatch();
}

/*!
 * \internal
 * \brief Returns a failed BuiltRequest carrying \a message.
 */
BuiltRequest failure(BuiltRequest built, const QString &message)
{
    built.ok = false;
    built.errorString = message;
    return built;
}
} // namespace

/*!
 * \brief Builds a network request from \a spec.
 *
 * Variables are resolved in the URL, enabled parameters, headers, form fields,
 * the raw body, and all authentication fields; every unknown name is collected
 * in BuiltRequest::unresolvedVariables. An unknown name in the URL fails the
 * build, because sending a literal "{{name}}" as host or path is never meant.
 *
 * A URL that does not start with "<scheme>://" is treated as http. Enabled
 * parameters (and an API key sent as query parameter) are percent-encoded and
 * appended to the query that the URL already carries. Headers keep their
 * order; repeated names are merged into the first occurrence (see
 * headerValueSeparator()), and values are sent as UTF-8. The authentication
 * tab overrides manual headers of the same name. The User-Agent and a
 * body-dependent Content-Type are added only when the user did not set them.
 * HEAD requests never carry a body.
 *
 * Qt itself adds Accept-Encoding, Accept-Language, and Connection headers, a
 * "Mozilla/5.0" User-Agent when none is set, and Cache-Control/Pragma
 * "no-cache" because of the AlwaysNetwork cache policy. Cookies are neither
 * loaded from nor saved to a cookie jar: only Cookie headers the user enters
 * are sent. Redirects are never followed by Qt; HttpExecutor follows them so
 * that credentials can be dropped when a redirect leaves the origin.
 *
 * \param spec The request as edited by the user.
 * \param variables The active environment variables.
 * \param options Application-wide defaults (User-Agent, timeout, redirects).
 * \return The built request, or one with ok == false and an error message.
 */
BuiltRequest RequestBuilder::build(const RequestSpec &spec, const VariableResolver &variables,
                                   const Options &options)
{
    BuiltRequest built;
    QStringList &unresolved = built.unresolvedVariables;
    const auto resolve = [&](const QString &text) { return variables.resolve(text, &unresolved); };
    const auto resolvePairs = [&](const QList<KeyValue> &rows) {
        QList<KeyValue> resolved;
        for (const KeyValue &row : rows) {
            if (row.enabled)
                resolved.append({resolve(row.key), resolve(row.value), true});
        }
        return resolved;
    };

    QStringList urlUnresolved;
    QString urlText = variables.resolve(spec.url.trimmed(), &urlUnresolved).trimmed();
    for (const QString &name : std::as_const(urlUnresolved))
        unresolved.append(name);

    QList<KeyValue> queryPairs = resolvePairs(spec.queryParams);
    if (spec.auth.type == AuthType::ApiKey && spec.auth.apiKeyLocation == ApiKeyLocation::Query)
        queryPairs.append({resolve(spec.auth.apiKeyName), resolve(spec.auth.apiKeyValue), true});

    if (urlText.isEmpty())
        return failure(std::move(built), QStringLiteral("URL is empty."));
    if (!urlUnresolved.isEmpty()) {
        QStringList references;
        for (const QString &name : std::as_const(urlUnresolved))
            references.append(QStringLiteral("{{%1}}").arg(name));
        return failure(std::move(built), QStringLiteral("Unresolved variable in URL: %1")
                                             .arg(references.join(QStringLiteral(", "))));
    }
    if (!hasScheme(urlText))
        urlText.prepend(QStringLiteral("http://"));

    QUrl url(urlText, QUrl::TolerantMode);
    if (!url.isValid() || url.host().isEmpty())
        // The text as typed: the resolved one may show the value of a secret variable.
        return failure(std::move(built), QStringLiteral("Invalid URL: %1").arg(spec.url.trimmed()));
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1StringView("http") && scheme != QLatin1StringView("https"))
        return failure(std::move(built), QStringLiteral("Unsupported URL scheme: %1").arg(scheme));

    const QByteArray extraQuery = encodeKeyValuePairs(queryPairs);
    if (!extraQuery.isEmpty()) {
        QString query = url.query(QUrl::FullyEncoded);
        if (!query.isEmpty())
            query += QLatin1Char('&');
        query += QString::fromLatin1(extraQuery);
        url.setQuery(query, QUrl::StrictMode);
    }

    // Qt's HTTP/1 backend keeps only the last of several headers with the same
    // name, so repeated names are merged into their first occurrence here.
    QList<std::pair<QString, QString>> manualHeaders;
    for (const KeyValue &row : resolvePairs(spec.headers)) {
        const QString name = row.key.trimmed();
        if (name.isEmpty())
            continue;
        const auto existing = std::find_if(manualHeaders.begin(), manualHeaders.end(),
                                           [&name](const std::pair<QString, QString> &header) {
                                               return header.first.compare(name, Qt::CaseInsensitive) == 0;
                                           });
        if (existing == manualHeaders.end())
            manualHeaders.append({name, row.value});
        else
            existing->second += headerValueSeparator(name) + row.value;
    }

    QHttpHeaders headers;
    for (const auto &[name, value] : std::as_const(manualHeaders)) {
        if (const QString error = appendHeader(headers, name, value); !error.isEmpty())
            return failure(std::move(built), error);
    }

    built.credentialHeaders = {QByteArrayLiteral("Authorization"),
                               QByteArrayLiteral("Proxy-Authorization"), QByteArrayLiteral("Cookie")};
    const QString authorization = QStringLiteral("Authorization");
    QString authError;
    switch (spec.auth.type) {
    case AuthType::None:
    case AuthType::Inherit: // Resolved from the folders before building; none is left here.
        break;
    case AuthType::Basic: {
        const QByteArray credentials =
            (resolve(spec.auth.username) + QLatin1Char(':') + resolve(spec.auth.password)).toUtf8();
        headers.removeAll(QHttpHeaders::WellKnownHeader::Authorization);
        authError = appendHeader(headers, authorization,
                                 QStringLiteral("Basic ") + QString::fromLatin1(credentials.toBase64()));
        break;
    }
    case AuthType::Bearer:
        headers.removeAll(QHttpHeaders::WellKnownHeader::Authorization);
        authError = appendHeader(headers, authorization,
                                 QStringLiteral("Bearer %1").arg(resolve(spec.auth.token)));
        break;
    case AuthType::ApiKey:
        if (spec.auth.apiKeyLocation == ApiKeyLocation::Header) {
            const QString name = resolve(spec.auth.apiKeyName).trimmed();
            if (name.isEmpty())
                break;
            headers.removeAll(name);
            authError = appendHeader(headers, name, resolve(spec.auth.apiKeyValue));
            built.credentialHeaders.append(name.toUtf8());
        }
        break;
    }
    if (!authError.isEmpty())
        return failure(std::move(built), authError);

    if (!options.userAgent.isEmpty() && !headers.contains(QHttpHeaders::WellKnownHeader::UserAgent)) {
        if (const QString error = appendHeader(headers, QStringLiteral("User-Agent"), options.userAgent);
            !error.isEmpty()) {
            return failure(std::move(built), error);
        }
    }

    if (spec.method != HttpMethod::Head && spec.body.mode != BodyMode::None) {
        if (spec.body.mode == BodyMode::Raw)
            built.body = resolve(spec.body.raw).toUtf8();
        else
            built.body = encodeKeyValuePairs(resolvePairs(spec.body.form));
        if (!headers.contains(QHttpHeaders::WellKnownHeader::ContentType)) {
            headers.append(QHttpHeaders::WellKnownHeader::ContentType,
                           defaultContentType(spec.body.mode, spec.body.rawType));
        }
    }

    built.verb = httpMethodVerb(spec.method);
    built.followRedirects = options.followRedirects;
    built.request.setUrl(url);
    built.request.setHeaders(std::move(headers));
    built.request.setTransferTimeout(std::chrono::milliseconds(
        spec.timeoutMs > 0 ? spec.timeoutMs : options.defaultTimeoutMs));
    built.request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                               QNetworkRequest::ManualRedirectPolicy);
    built.request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                               QNetworkRequest::AlwaysNetwork);
    built.request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    built.request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    built.ok = true;
    return built;
}
