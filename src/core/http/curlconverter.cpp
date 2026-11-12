// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/curlconverter.h"

#include <QSet>
#include <QUrl>

#include <algorithm>
#include <optional>

#include "core/http/httpencoding.h"

namespace {
/*!
 * \internal
 * \brief Returns \a value double-quoted for curl -F, so ';', '@', and '<' stay text.
 */
QString formQuoted(const QString &value)
{
    QString escaped = value;
    escaped.replace(u'\\', QStringLiteral("\\\\")).replace(u'"', QStringLiteral("\\\""));
    return u'"' + escaped + u'"';
}

/*!
 * \internal
 * \brief Returns the value of hexadecimal digit \a c, or -1.
 */
int hexDigit(QChar c)
{
    if (c >= u'0' && c <= u'9')
        return c.unicode() - u'0';
    if (c >= u'a' && c <= u'f')
        return c.unicode() - u'a' + 10;
    if (c >= u'A' && c <= u'F')
        return c.unicode() - u'A' + 10;
    return -1;
}

/*!
 * \internal
 * \brief Decodes a bash $'...' word whose content starts at \a start and appends it to \a out.
 *
 * Supports every bash ANSI-C escape: \a \b \e \E \f \n \r \t \v \\ \' \" \?,
 * octal \nnn (one to three digits), \xHH (one or two hex digits), \uHHHH
 * (one to four), \UHHHHHHHH (one to eight), and \cX control characters.
 * Browsers rely on these: Chrome writes '!' as !, Firefox writes
 * non-ASCII text as \xHH bytes. Octal and \x escapes produce raw bytes, so the
 * word is assembled as UTF-8 and decoded once at the closing quote.
 * \return The index after the closing quote, or -1 when it is missing.
 */
qsizetype readAnsiCQuoted(const QString &text, qsizetype start, QString &out)
{
    QByteArray bytes;
    const qsizetype size = text.size();
    qsizetype i = start;

    const auto readNumber = [&](int base, int maxDigits, char32_t &value) {
        int digits = 0;
        value = 0;
        while (digits < maxDigits && i < size) {
            const int digit = hexDigit(text.at(i));
            if (digit < 0 || digit >= base)
                break;
            value = value * base + digit;
            ++i;
            ++digits;
        }
        return digits;
    };

    while (i < size) {
        const QChar c = text.at(i);
        if (c == u'\'') {
            out += QString::fromUtf8(bytes);
            return i + 1;
        }
        if (c != u'\\' || i + 1 >= size) {
            const qsizetype length = c.isHighSurrogate() && i + 1 < size ? 2 : 1;
            bytes += QStringView(text).sliced(i, length).toUtf8();
            i += length;
            continue;
        }

        const QChar escape = text.at(i + 1);
        i += 2;
        char32_t value = 0;
        switch (escape.unicode()) {
        case u'a':
            bytes += '\a';
            break;
        case u'b':
            bytes += '\b';
            break;
        case u'e':
        case u'E':
            bytes += '\x1b';
            break;
        case u'f':
            bytes += '\f';
            break;
        case u'n':
            bytes += '\n';
            break;
        case u'r':
            bytes += '\r';
            break;
        case u't':
            bytes += '\t';
            break;
        case u'v':
            bytes += '\v';
            break;
        case u'\\':
        case u'\'':
        case u'"':
        case u'?':
            bytes += char(escape.unicode());
            break;
        case u'x':
            if (readNumber(16, 2, value) > 0)
                bytes += char(value);
            else
                bytes += "\\x";
            break;
        case u'u':
        case u'U':
            if (readNumber(16, escape == u'u' ? 4 : 8, value) > 0)
                bytes += QString::fromUcs4(&value, 1).toUtf8();
            else
                bytes += QByteArray("\\") + char(escape.unicode());
            break;
        case u'c':
            if (i < size) {
                bytes += char(text.at(i).unicode() & 0x1F);
                ++i;
            } else {
                bytes += "\\c";
            }
            break;
        default:
            if (escape >= u'0' && escape <= u'7') {
                --i;
                readNumber(8, 3, value);
                bytes += char(value & 0xFF);
            } else {
                bytes += '\\';
                bytes += QString(escape).toUtf8();
            }
            break;
        }
    }
    return -1;
}

/*!
 * \internal
 * \brief Splits a bash command line into words.
 *
 * Supports single quotes (literal), double quotes (with \", \\, \$, and \`
 * escapes), ANSI-C quotes $'...' (see readAnsiCQuoted()), backslash escapes
 * outside quotes, and backslash-newline continuations.
 * Line endings must already be normalized to LF.
 * \param text The command line.
 * \return The words, or std::nullopt for an unterminated quote.
 */
std::optional<QStringList> tokenize(const QString &text)
{
    QStringList tokens;
    QString current;
    bool inToken = false;
    const qsizetype size = text.size();
    qsizetype i = 0;

    const auto isSpace = [](QChar c) {
        return c == u' ' || c == u'\t' || c == u'\n';
    };

    while (i < size) {
        const QChar c = text.at(i);
        if (isSpace(c)) {
            if (inToken) {
                tokens.append(current);
                current.clear();
                inToken = false;
            }
            ++i;
            continue;
        }

        if (c == u'\\') {
            if (i + 1 < size && text.at(i + 1) == u'\n') {
                i += 2;
                continue;
            }
            inToken = true;
            if (i + 1 < size)
                current += text.at(i + 1);
            i += 2;
            continue;
        }

        inToken = true;
        if (c == u'\'') {
            const qsizetype end = text.indexOf(u'\'', i + 1);
            if (end < 0)
                return std::nullopt;
            current += QStringView(text).sliced(i + 1, end - i - 1);
            i = end + 1;
            continue;
        }

        if (c == u'"') {
            ++i;
            bool closed = false;
            while (i < size) {
                const QChar d = text.at(i);
                if (d == u'"') {
                    closed = true;
                    ++i;
                    break;
                }
                if (d == u'\\' && i + 1 < size) {
                    const QChar next = text.at(i + 1);
                    if (next == u'\n') {
                        i += 2;
                        continue;
                    }
                    if (next == u'"' || next == u'\\' || next == u'$' || next == u'`') {
                        current += next;
                        i += 2;
                        continue;
                    }
                }
                current += d;
                ++i;
            }
            if (!closed)
                return std::nullopt;
            continue;
        }

        if (c == u'$' && i + 1 < size && text.at(i + 1) == u'\'') {
            i = readAnsiCQuoted(text, i + 2, current);
            if (i < 0)
                return std::nullopt;
            continue;
        }

        current += c;
        ++i;
    }

    if (inToken)
        tokens.append(current);
    return tokens;
}

/*!
 * \internal
 * \brief Returns \a text as one single-quoted bash word.
 */
QString shellQuote(QString text)
{
    text.replace(u'\'', QStringLiteral("'\\''"));
    return u'\'' + text + u'\'';
}

/*!
 * \internal
 * \brief Returns the value of the first enabled header named \a name, case-insensitively.
 */
std::optional<QString> headerValue(const QList<KeyValue> &headers, QStringView name)
{
    for (const KeyValue &header : headers) {
        if (header.enabled && header.key.trimmed().compare(name, Qt::CaseInsensitive) == 0)
            return header.value;
    }
    return std::nullopt;
}

/*!
 * \internal
 * \brief Decodes form data such as "a=1&b=x+y" into fields.
 *
 * '+' decodes to a space before percent-decoding, as browsers and servers
 * treat application/x-www-form-urlencoded data.
 * \return The fields, or std::nullopt when a segment has no '='.
 */
std::optional<QList<KeyValue>> decodeFormData(const QString &data)
{
    const auto decode = [](QString part) {
        part.replace(u'+', u' ');
        return QUrl::fromPercentEncoding(part.toUtf8());
    };

    QList<KeyValue> fields;
    for (const QString &segment : data.split(u'&')) {
        const qsizetype equals = segment.indexOf(u'=');
        if (equals < 0)
            return std::nullopt;
        fields.append({decode(segment.left(equals)), decode(segment.mid(equals + 1)), true});
    }
    return fields;
}

/*!
 * \internal
 * \brief Options without a RequestSpec equivalent whose value is consumed and dropped.
 *
 * Flags without a value (for example -L, -k, -s) need no table: every unknown
 * option is recorded as ignored and consumes nothing.
 */
const QSet<QString> kIgnoredWithValue {
    QStringLiteral("-o"), QStringLiteral("--output"), QStringLiteral("-m"),
    QStringLiteral("--max-time"), QStringLiteral("--connect-timeout"), QStringLiteral("--retry"),
    QStringLiteral("--retry-delay"), QStringLiteral("--retry-max-time"), QStringLiteral("-w"),
    QStringLiteral("--write-out"), QStringLiteral("-x"), QStringLiteral("--proxy"),
    QStringLiteral("-U"), QStringLiteral("--proxy-user"), QStringLiteral("--noproxy"),
    QStringLiteral("-c"), QStringLiteral("--cookie-jar"), QStringLiteral("--cacert"),
    QStringLiteral("--capath"), QStringLiteral("-E"), QStringLiteral("--cert"),
    QStringLiteral("--cert-type"), QStringLiteral("--key"), QStringLiteral("--key-type"),
    QStringLiteral("--pass"), QStringLiteral("-K"), QStringLiteral("--config"),
    QStringLiteral("--max-redirs"), QStringLiteral("-r"), QStringLiteral("--range"),
    QStringLiteral("--resolve"), QStringLiteral("--connect-to"), QStringLiteral("--interface"),
    QStringLiteral("--limit-rate"), QStringLiteral("-y"), QStringLiteral("--speed-time"),
    QStringLiteral("-Y"), QStringLiteral("--speed-limit"), QStringLiteral("-D"),
    QStringLiteral("--dump-header"), QStringLiteral("--trace"), QStringLiteral("--trace-ascii"),
    QStringLiteral("--stderr"), QStringLiteral("--unix-socket"),
    QStringLiteral("--abstract-unix-socket"), QStringLiteral("--expect100-timeout"),
    QStringLiteral("--dns-servers"), QStringLiteral("--ciphers"), QStringLiteral("--tls-max"),
    QStringLiteral("--local-port"), QStringLiteral("--keepalive-time"),
};

/*!
 * \internal
 * \brief Short options that take a value, which may be attached as in -XPOST.
 */
const QSet<QString> kShortWithValue {
    QStringLiteral("-X"), QStringLiteral("-H"), QStringLiteral("-d"), QStringLiteral("-u"),
    QStringLiteral("-A"), QStringLiteral("-b"), QStringLiteral("-e"), QStringLiteral("-o"),
    QStringLiteral("-m"), QStringLiteral("-w"), QStringLiteral("-x"), QStringLiteral("-U"),
    QStringLiteral("-c"), QStringLiteral("-E"), QStringLiteral("-K"), QStringLiteral("-r"),
    QStringLiteral("-y"), QStringLiteral("-Y"), QStringLiteral("-D"), QStringLiteral("-F"),
    QStringLiteral("-T"),
};

/*!
 * \internal
 * \brief Returns the header set by the cURL option \a name, or an empty string.
 *
 * -A/--user-agent, -b/--cookie, and -e/--referer are shorthands for headers.
 */
QString headerForOption(const QString &name)
{
    if (name == QLatin1StringView("-A") || name == QLatin1StringView("--user-agent"))
        return QStringLiteral("User-Agent");
    if (name == QLatin1StringView("-b") || name == QLatin1StringView("--cookie"))
        return QStringLiteral("Cookie");
    if (name == QLatin1StringView("-e") || name == QLatin1StringView("--referer"))
        return QStringLiteral("Referer");
    return {};
}

/*!
 * \internal
 * \brief Returns a failed ParseResult carrying \a message.
 */
CurlConverter::ParseResult failure(const QString &message)
{
    CurlConverter::ParseResult result;
    result.errorString = message;
    return result;
}
} // namespace

/*!
 * \brief Converts a cURL command line into a RequestSpec.
 *
 * The first word must be "curl" or "curl.exe". The URL is kept verbatim,
 * including its query string. The method is HEAD for -I, otherwise the -X
 * value, otherwise POST when data is present, otherwise GET.
 *
 * Data from all -d/--data* options, --data-urlencode (encoded as curl does),
 * and --json is joined with '&'. With -G the data is appended to the URL query
 * instead. --json also adds JSON Content-Type and Accept headers unless set,
 * --oauth2-bearer adds an Authorization header, and --compressed removes a
 * copied Accept-Encoding header. -F and --form-string become form data parts,
 * "name=@path" a file part with an optional ";type="; --data-binary @file and
 * -T file become a file body (-T defaults to PUT). Text from a file
 * (name=<file), -d @file, which curl changes, mixing these with -d, and
 * commands in Windows cmd syntax are rejected. The body
 * mode follows the Content-Type header: JSON or XML types produce a raw body
 * of that flavor; form-urlencoded or no type produce form fields when every
 * segment is "key=value", and otherwise raw text. When curl would have sent its
 * default form Content-Type for such raw text, that header is added
 * explicitly so the request stays equivalent.
 *
 * Options without a RequestSpec equivalent are listed in
 * ParseResult::ignoredOptions; known ones that take a value consume it.
 * \param command The command line; CRLF line endings are accepted.
 * \return The converted request, or a result with ok == false and a message.
 */
CurlConverter::ParseResult CurlConverter::parse(const QString &command)
{
    QString normalized = command;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(u'\r', u'\n');

    // "Copy as cURL (cmd)" escapes quotes as ^"; bash quoting would misread it.
    if (normalized.contains(QStringLiteral("^\"")))
        return failure(QStringLiteral("Windows cmd syntax is not supported; copy the command as cURL (bash)."));

    const std::optional<QStringList> tokenized = tokenize(normalized);
    if (!tokenized)
        return failure(QStringLiteral("Unterminated quote."));
    const QStringList &tokens = *tokenized;
    if (tokens.isEmpty()
        || (tokens.first().compare(QLatin1StringView("curl"), Qt::CaseInsensitive) != 0
            && tokens.first().compare(QLatin1StringView("curl.exe"), Qt::CaseInsensitive) != 0)) {
        return failure(QStringLiteral("Not a cURL command."));
    }

    ParseResult result;
    RequestSpec &spec = result.request;
    std::optional<HttpMethod> explicitMethod;
    bool head = false;
    bool get = false;
    bool json = false;
    bool compressed = false;
    QStringList data;
    QList<FormPart> parts;
    bool form = false;
    std::optional<QString> uploadFile;
    bool uploadIsPut = false;

    for (qsizetype i = 1; i < tokens.size(); ++i) {
        const QString &token = tokens.at(i);

        if (!token.startsWith(u'-') || token == QLatin1StringView("-")) {
            if (spec.url.isEmpty())
                spec.url = token;
            else
                result.ignoredOptions.append(token);
            continue;
        }

        QString name = token;
        std::optional<QString> inlineValue;
        if (token.startsWith(QLatin1StringView("--"))) {
            const qsizetype equals = token.indexOf(u'=');
            if (equals > 0) {
                name = token.left(equals);
                inlineValue = token.mid(equals + 1);
            }
        } else if (token.size() > 2) {
            name = token.left(2);
            if (kShortWithValue.contains(name)) {
                inlineValue = token.mid(2);
            } else {
                // Combined short flags such as -sSL; only -I has a meaning here.
                if (token.contains(u'I'))
                    head = true;
                result.ignoredOptions.append(token);
                continue;
            }
        }

        const auto takeValue = [&]() -> std::optional<QString> {
            if (inlineValue)
                return inlineValue;
            if (i + 1 < tokens.size())
                return tokens.at(++i);
            return std::nullopt;
        };
        const auto missingValue = [&name]() {
            return failure(QStringLiteral("Missing value for option: %1").arg(name));
        };

        if (name == QLatin1StringView("-I") || name == QLatin1StringView("--head")) {
            head = true;
        } else if (name == QLatin1StringView("-X") || name == QLatin1StringView("--request")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            explicitMethod = httpMethodFromString(*value);
            if (!explicitMethod)
                return failure(QStringLiteral("Unsupported method: %1").arg(*value));
        } else if (name == QLatin1StringView("-H") || name == QLatin1StringView("--header")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            const qsizetype colon = value->indexOf(u':');
            if (colon <= 0) {
                result.ignoredOptions.append(name + u' ' + *value);
                continue;
            }
            spec.headers.append({value->left(colon).trimmed(), value->mid(colon + 1).trimmed(), true});
        } else if (name == QLatin1StringView("-d") || name == QLatin1StringView("--data")
                   || name == QLatin1StringView("--data-binary")
                   || name == QLatin1StringView("--data-ascii")
                   || name == QLatin1StringView("--data-raw")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            if (name == QLatin1StringView("--data-binary") && *value == QLatin1StringView("@-"))
                return failure(QStringLiteral("Data from standard input (@-) is not supported."));
            if (name == QLatin1StringView("--data-binary") && value->startsWith(u'@') && value->size() > 1) {
                // Sent byte for byte, which is what a file body does.
                if (uploadFile)
                    return failure(QStringLiteral("Only one file can be sent as the body."));
                uploadFile = value->mid(1);
                continue;
            }
            if (name != QLatin1StringView("--data-raw") && value->startsWith(u'@'))
                return failure(QStringLiteral("Files read with -d @file are changed by curl; use --data-binary @file."));
            data.append(*value);
        } else if (name == QLatin1StringView("--data-urlencode")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            // curl splits at the first '=' or '@': "name=content" encodes the
            // content only, "=content" and "content" encode everything, and
            // '@' reads a file.
            const auto separator = std::find_if(value->cbegin(), value->cend(),
                                                [](QChar c) { return c == u'=' || c == u'@'; });
            if (separator != value->cend() && *separator == u'@')
                return failure(QStringLiteral("File uploads (@file) are not supported."));
            if (separator == value->cend()) {
                data.append(QString::fromLatin1(QUrl::toPercentEncoding(*value)));
            } else {
                const qsizetype at = separator - value->cbegin();
                const QString key = value->left(at);
                const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(value->mid(at + 1)));
                data.append(key.isEmpty() ? encoded : key + u'=' + encoded);
            }
        } else if (name == QLatin1StringView("--json")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            if (value->startsWith(u'@'))
                return failure(QStringLiteral("File uploads (@file) are not supported."));
            data.append(*value);
            json = true;
        } else if (name == QLatin1StringView("-G") || name == QLatin1StringView("--get")) {
            get = true;
        } else if (name == QLatin1StringView("--compressed")) {
            compressed = true;
            result.ignoredOptions.append(token);
        } else if (name == QLatin1StringView("-F") || name == QLatin1StringView("--form")
                   || name == QLatin1StringView("--form-string")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            const qsizetype equals = value->indexOf(u'=');
            if (equals <= 0)
                return failure(QStringLiteral("Invalid form field: %1").arg(*value));
            FormPart part;
            part.key = value->left(equals);
            QString content = value->mid(equals + 1);
            if (name != QLatin1StringView("--form-string")) {
                if (content.startsWith(u'<'))
                    return failure(QStringLiteral("Form text read from a file (name=<file) is not supported."));
                // curl reads ";type=" and ";filename=" after the content, which
                // may be double-quoted to hold ';' or a leading '@' or '<'.
                QString quoted;
                const bool isQuoted = content.startsWith(u'"');
                if (isQuoted) {
                    qsizetype i = 1;
                    for (; i < content.size() && content.at(i) != u'"'; ++i) {
                        if (content.at(i) == u'\\' && i + 1 < content.size())
                            ++i;
                        quoted.append(content.at(i));
                    }
                    if (i >= content.size())
                        return failure(QStringLiteral("Unterminated quote in form field %1.").arg(part.key));
                    content = content.mid(i + 1);
                }
                QStringList pieces = content.split(u';');
                content = pieces.takeFirst();
                for (const QString &piece : std::as_const(pieces)) {
                    if (piece.startsWith(QLatin1StringView("type=")))
                        part.contentType = piece.mid(5);
                    else
                        result.ignoredOptions.append(name + u' ' + part.key + u';' + piece);
                }
                if (isQuoted) {
                    content = quoted;
                } else if (content.startsWith(u'@')) {
                    part.isFile = true;
                    content = content.mid(1);
                }
            }
            part.value = content;
            parts.append(part);
            form = true;
        } else if (name == QLatin1StringView("-T") || name == QLatin1StringView("--upload-file")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            if (uploadFile)
                return failure(QStringLiteral("Only one file can be sent as the body."));
            if (*value == QLatin1StringView("-") || value->contains(u'{') || value->contains(u'['))
                return failure(QStringLiteral("Uploads from standard input or of several files (-T) are not supported."));
            uploadFile = *value;
            uploadIsPut = true;
        } else if (name == QLatin1StringView("--oauth2-bearer")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            spec.headers.append({QStringLiteral("Authorization"), QStringLiteral("Bearer ") + *value, true});
        } else if (name == QLatin1StringView("-u") || name == QLatin1StringView("--user")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            const qsizetype colon = value->indexOf(u':');
            spec.auth.type = AuthType::Basic;
            spec.auth.username = colon < 0 ? *value : value->left(colon);
            spec.auth.password = colon < 0 ? QString() : value->mid(colon + 1);
        } else if (name == QLatin1StringView("--url")) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            spec.url = *value;
        } else if (const QString header = headerForOption(name); !header.isEmpty()) {
            const std::optional<QString> value = takeValue();
            if (!value)
                return missingValue();
            spec.headers.append({header, *value, true});
        } else if (kIgnoredWithValue.contains(name)) {
            if (!takeValue())
                return missingValue();
            result.ignoredOptions.append(name);
        } else {
            result.ignoredOptions.append(token);
        }
    }

    if (spec.url.isEmpty())
        return failure(QStringLiteral("No URL found."));

    if (get && !data.isEmpty()) {
        spec.url += (spec.url.contains(u'?') ? u'&' : u'?') + data.join(u'&');
        data.clear();
    }
    if (compressed) {
        // --compressed negotiates the encoding itself; a copied Accept-Encoding
        // header would switch off Qt's transparent decompression.
        spec.headers.removeIf([](const KeyValue &header) {
            return header.key.compare(QLatin1StringView("Accept-Encoding"), Qt::CaseInsensitive) == 0;
        });
    }
    if (json) {
        if (!headerValue(spec.headers, u"Content-Type"))
            spec.headers.append({QStringLiteral("Content-Type"), QStringLiteral("application/json"), true});
        if (!headerValue(spec.headers, u"Accept"))
            spec.headers.append({QStringLiteral("Accept"), QStringLiteral("application/json"), true});
    }

    if (int(form) + int(uploadFile.has_value()) + int(!data.isEmpty()) > 1)
        return failure(QStringLiteral("Form fields (-F), a file body, and data (-d) cannot be combined."));

    if (head)
        spec.method = HttpMethod::Head;
    else if (explicitMethod)
        spec.method = *explicitMethod;
    else if (uploadIsPut)
        spec.method = HttpMethod::Put;
    else
        spec.method = data.isEmpty() && !form && !uploadFile ? HttpMethod::Get : HttpMethod::Post;

    if (form) {
        spec.body.mode = BodyMode::FormData;
        spec.body.parts = parts;
    } else if (uploadFile) {
        spec.body.mode = BodyMode::File;
        spec.body.filePath = *uploadFile;
    }

    if (!data.isEmpty() && spec.method != HttpMethod::Head) {
        const QString joined = data.join(u'&');
        const std::optional<QString> contentType =
            headerValue(spec.headers, u"Content-Type");
        const QString type = contentType.value_or(QString()).toLower();

        spec.body.mode = BodyMode::Raw;
        spec.body.raw = joined;
        if (type.contains(QLatin1StringView("json"))) {
            spec.body.rawType = RawBodyType::Json;
        } else if (type.contains(QLatin1StringView("xml"))) {
            spec.body.rawType = RawBodyType::Xml;
        } else if (!contentType || type.contains(QLatin1StringView("x-www-form-urlencoded"))) {
            if (const std::optional<QList<KeyValue>> fields = decodeFormData(joined)) {
                spec.body.mode = BodyMode::FormUrlEncoded;
                spec.body.raw.clear();
                spec.body.form = *fields;
            } else {
                spec.body.rawType = RawBodyType::Text;
                if (!contentType) {
                    spec.headers.append({QStringLiteral("Content-Type"),
                                         QStringLiteral("application/x-www-form-urlencoded"), true});
                }
            }
        } else {
            spec.body.rawType = RawBodyType::Text;
        }
    }

    result.ok = true;
    return result;
}

/*!
 * \brief Returns a multi-line bash cURL command equivalent to \a spec.
 *
 * Each argument after "curl" starts a new line joined by a backslash
 * continuation, and every value is single-quoted. -X is omitted only for GET
 * without a body (curl's default); HEAD uses --head, because "-X HEAD" makes
 * curl wait for a body. Enabled query parameters and an API key sent as query
 * parameter are percent-encoded onto the URL. A raw body without an explicit
 * Content-Type gets the same default header that RequestBuilder would send;
 * form bodies rely on curl's own form default.
 * \param spec The request to export; variables stay unresolved.
 */
QString CurlConverter::toCurl(const RequestSpec &spec, bool followRedirects, int defaultTimeoutMs)
{
    QStringList lines {QStringLiteral("curl")};
    const bool hasBody = spec.method != HttpMethod::Head && spec.body.mode != BodyMode::None;

    // curl follows no redirect and waits forever unless told otherwise.
    if (followRedirects)
        lines.append(QStringLiteral("-L"));
    const int timeoutMs = spec.timeoutMs > 0 ? spec.timeoutMs : defaultTimeoutMs;
    if (timeoutMs > 0)
        lines.append(QStringLiteral("-m ") + QString::number(timeoutMs / 1000.0, 'g', 10));

    if (spec.method == HttpMethod::Head)
        lines.append(QStringLiteral("--head"));
    else if (spec.method != HttpMethod::Get || hasBody)
        lines.append(QStringLiteral("-X ") + shellQuote(QString::fromLatin1(httpMethodVerb(spec.method))));

    QList<KeyValue> queryPairs = spec.queryParams;
    if (spec.auth.type == AuthType::ApiKey && spec.auth.apiKeyLocation == ApiKeyLocation::Query)
        queryPairs.append({spec.auth.apiKeyName, spec.auth.apiKeyValue, true});
    QString url = spec.url;
    const QByteArray query = encodeKeyValuePairs(queryPairs);
    if (!query.isEmpty())
        url += (url.contains(u'?') ? u'&' : u'?') + QString::fromLatin1(query);
    lines.append(shellQuote(url));

    for (const KeyValue &header : spec.headers) {
        if (header.enabled && !header.key.trimmed().isEmpty())
            lines.append(QStringLiteral("-H ") + shellQuote(header.key.trimmed() + u": " + header.value));
    }

    switch (spec.auth.type) {
    case AuthType::None:
    case AuthType::Inherit:
        break;
    case AuthType::Basic:
        lines.append(QStringLiteral("-u ") + shellQuote(spec.auth.username + u':' + spec.auth.password));
        break;
    case AuthType::Bearer:
        lines.append(QStringLiteral("-H ") + shellQuote(u"Authorization: Bearer " + spec.auth.token));
        break;
    case AuthType::ApiKey:
        if (spec.auth.apiKeyLocation == ApiKeyLocation::Header && !spec.auth.apiKeyName.trimmed().isEmpty()) {
            lines.append(QStringLiteral("-H ")
                         + shellQuote(spec.auth.apiKeyName.trimmed() + u": " + spec.auth.apiKeyValue));
        }
        break;
    }

    if (hasBody) {
        if (spec.body.mode == BodyMode::Raw) {
            if (!headerValue(spec.headers, u"Content-Type")) {
                const QString type = QString::fromLatin1(defaultContentType(spec.body.mode, spec.body.rawType));
                lines.append(QStringLiteral("-H ") + shellQuote(u"Content-Type: " + type));
            }
            lines.append(QStringLiteral("--data-raw ") + shellQuote(spec.body.raw));
        } else if (spec.body.mode == BodyMode::FormUrlEncoded) {
            lines.append(QStringLiteral("--data-raw ")
                         + shellQuote(QString::fromLatin1(encodeKeyValuePairs(spec.body.form))));
        } else if (spec.body.mode == BodyMode::FormData) {
            for (const FormPart &part : spec.body.parts) {
                if (!part.enabled || part.key.trimmed().isEmpty())
                    continue;
                const QString type = part.contentType.isEmpty() ? QString() : u";type=" + part.contentType;
                if (part.isFile)
                    lines.append(QStringLiteral("-F ") + shellQuote(part.key + u"=@" + part.value + type));
                else if (type.isEmpty())
                    lines.append(QStringLiteral("--form-string ") + shellQuote(part.key + u'=' + part.value));
                else
                    lines.append(QStringLiteral("-F ") + shellQuote(part.key + u'=' + formQuoted(part.value) + type));
            }
        } else {
            // curl would send its form type for --data-binary; ReqDeck sends one from the name.
            if (!headerValue(spec.headers, u"Content-Type")) {
                lines.append(QStringLiteral("-H ")
                             + shellQuote(u"Content-Type: " + QString::fromLatin1(contentTypeForFile(spec.body.filePath))));
            }
            lines.append(QStringLiteral("--data-binary ") + shellQuote(u'@' + spec.body.filePath));
        }
    }

    return lines.join(QStringLiteral(" \\\n  "));
}
