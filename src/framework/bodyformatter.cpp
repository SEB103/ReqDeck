// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "framework/bodyformatter.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QLatin1StringView>
#include <QList>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QStringList>
#include <QXmlStreamReader>

#include <optional>
#include <utility>

namespace {
/*!
 * \internal
 * \brief Number of leading bytes searched for a NUL byte.
 */
constexpr qsizetype kBinaryProbeBytes = 8192;

/*!
 * \internal
 * \brief Spaces per nesting level of formatted text.
 */
constexpr int kIndentWidth = 2;

/*!
 * \internal
 * \brief Longest formatted text; indentation grows with depth times lines.
 */
constexpr qsizetype kMaxFormattedChars = 4 * kMaxDisplayChars;

/*!
 * \internal
 * \brief Returns the lower-case media type of \a contentType, without parameters.
 */
QString mediaTypeOf(const QString &contentType)
{
    return contentType.section(u';', 0, 0).trimmed().toLower();
}

/*!
 * \internal
 * \brief Returns the charset parameter of \a contentType; empty when there is none.
 */
QString charsetOf(const QString &contentType)
{
    const QStringList parts = contentType.split(u';');
    for (qsizetype i = 1; i < parts.size(); ++i) {
        const QString name = parts.at(i).section(u'=', 0, 0).trimmed();
        if (name.compare(QLatin1StringView("charset"), Qt::CaseInsensitive) != 0)
            continue;
        QString charset = parts.at(i).section(u'=', 1).trimmed();
        charset.remove(u'"');
        charset.remove(u'\'');
        return charset;
    }
    return {};
}

/*!
 * \internal
 * \brief Returns the encoding named by an XML declaration at the start of \a body.
 */
QString xmlDeclaredEncoding(const QByteArray &body)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^\s*<\?xml[^>]*?encoding\s*=\s*["']([A-Za-z0-9._\-]+)["'])"));
    const QString start = QString::fromLatin1(body.first(qMin(body.size(), qsizetype(256))));
    return pattern.match(start).captured(1);
}

/*!
 * \internal
 * \brief Returns whether \a charset uses more than one byte for ASCII characters.
 *
 * Such text is full of NUL bytes, which otherwise mark binary data.
 */
bool isWideCharset(const QString &charset)
{
    return charset.startsWith(QLatin1StringView("utf-16"), Qt::CaseInsensitive)
           || charset.startsWith(QLatin1StringView("utf-32"), Qt::CaseInsensitive)
           || charset.startsWith(QLatin1StringView("ucs-2"), Qt::CaseInsensitive)
           || charset.startsWith(QLatin1StringView("ucs-4"), Qt::CaseInsensitive);
}

/*!
 * \internal
 * \brief Returns whether \a text is valid JSON.
 *
 * QJsonDocument refuses a number beyond the range of a double, such as 1e400,
 * which JSON itself allows. The check therefore runs on a copy in which every
 * exponent outside strings is 0; the formatted text keeps the original.
 */
bool isJson(const QString &text)
{
    static const QRegularExpression exponent(QStringLiteral(R"((?<=[0-9])([eE])[+-]?[0-9]+)"));
    QString checked;
    checked.reserve(text.size());
    qsizetype start = 0;
    bool inString = false;
    for (qsizetype i = 0; i <= text.size(); ++i) {
        const bool end = i == text.size();
        if (!end && text.at(i) == u'\\' && inString) {
            ++i;
            continue;
        }
        if (end || text.at(i) == u'"') {
            QString part = text.mid(start, i - start + (end ? 0 : 1));
            if (!inString)
                part.replace(exponent, QStringLiteral("\\1""0"));
            checked += part;
            start = i + 1;
            inString = !inString;
        }
    }

    QJsonParseError error;
    QJsonDocument::fromJson(checked.toUtf8(), &error);
    return error.error == QJsonParseError::NoError;
}

/*!
 * \internal
 * \brief One token of an XML document, located in the source text.
 */
struct XmlToken
{
    QXmlStreamReader::TokenType type {QXmlStreamReader::NoToken};
    qsizetype begin {0};
    qsizetype end {0};
    qsizetype endElement {-1};
    bool hasChildren {false};
    bool keepVerbatim {false};
};

/*!
 * \internal
 * \brief Returns \a text indented; nullopt when it is not well-formed.
 *
 * QXmlStreamWriter would rewrite the document: it normalizes the declaration,
 * quotes, character references, and namespace prefixes, and expands defaults.
 * So the reader only finds the tokens, their source text is copied unchanged,
 * and only whitespace between elements changes. An element with text of its own
 * or xml:space="preserve" is copied as a whole, because whitespace matters there.
 * The reader also rejects trailing garbage and unbalanced tags, so the same
 * function decides whether a body is XML at all.
 */
std::optional<QString> indentXml(const QString &text)
{
    QList<XmlToken> tokens;
    QList<qsizetype> open;
    qsizetype offset = 0;

    QXmlStreamReader reader(text);
    while (!reader.atEnd()) {
        const QXmlStreamReader::TokenType type = reader.readNext();
        if (reader.hasError())
            return std::nullopt;

        // Without a declaration the reader has looked ahead at the first token.
        if (type == QXmlStreamReader::StartDocument && reader.documentVersion().isEmpty())
            continue;

        XmlToken token;
        token.type = type;
        token.begin = offset;
        token.end = reader.characterOffset();
        offset = token.end;
        const qsizetype index = tokens.size();

        switch (type) {
        case QXmlStreamReader::StartElement:
            token.keepVerbatim = reader.attributes().value(QLatin1StringView("xml:space"))
                                 == QLatin1StringView("preserve");
            if (!open.isEmpty())
                tokens[open.last()].hasChildren = true;
            open.append(index);
            break;
        case QXmlStreamReader::EndElement: {
            XmlToken &start = tokens[open.takeLast()];
            start.endElement = index;
            start.keepVerbatim = start.keepVerbatim || !start.hasChildren;
            break;
        }
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::EntityReference:
            if (!open.isEmpty() && !QStringView(text).sliced(token.begin, token.end - token.begin)
                                        .trimmed().isEmpty()) {
                tokens[open.last()].keepVerbatim = true;
            }
            break;
        case QXmlStreamReader::Comment:
        case QXmlStreamReader::ProcessingInstruction:
            if (!open.isEmpty())
                tokens[open.last()].hasChildren = true;
            break;
        default:
            break;
        }
        tokens.append(token);
    }

    QString output;
    int depth = 0;
    const auto writeLine = [&](qsizetype begin, qsizetype end) {
        const QStringView source = QStringView(text).sliced(begin, end - begin).trimmed();
        if (source.isEmpty())
            return;
        if (!output.isEmpty())
            output += u'\n';
        output += QString(depth * kIndentWidth, u' ');
        output += source;
    };

    for (qsizetype i = 0; i < tokens.size(); ++i) {
        const XmlToken &token = tokens.at(i);
        switch (token.type) {
        case QXmlStreamReader::StartElement:
            if (token.keepVerbatim) {
                writeLine(token.begin, tokens.at(token.endElement).end);
                i = token.endElement;
            } else {
                writeLine(token.begin, token.end);
                ++depth;
            }
            break;
        case QXmlStreamReader::EndElement:
            --depth;
            writeLine(token.begin, token.end);
            break;
        case QXmlStreamReader::Characters:
        case QXmlStreamReader::EndDocument:
            break;
        default:
            writeLine(token.begin, token.end);
            break;
        }
    }
    return output;
}

/*!
 * \internal
 * \brief Detects the kind of the decoded \a text.
 *
 * The media type decides when it names JSON or XML. HTML stays text: it is
 * rarely well-formed XML. Anything else is recognized by its content, because
 * many servers label JSON as text/plain. Very long text is not parsed for that:
 * it could not be formatted anyway.
 */
BodyKind detectKind(const QString &text, const QString &mediaType)
{
    if (mediaType.contains(QLatin1StringView("json")))
        return BodyKind::Json;
    if (mediaType.contains(QLatin1StringView("xml")))
        return BodyKind::Xml;
    if (mediaType == QLatin1StringView("text/html") || text.size() > kMaxDisplayChars)
        return BodyKind::Text;

    const QStringView trimmed = QStringView(text).trimmed();
    if (trimmed.isEmpty())
        return BodyKind::Text;
    if ((trimmed.front() == u'{' || trimmed.front() == u'[') && isJson(text))
        return BodyKind::Json;
    if (trimmed.front() == u'<' && indentXml(text))
        return BodyKind::Xml;
    return BodyKind::Text;
}
} // namespace

/*!
 * \brief Decodes \a body for display and detects what it contains.
 *
 * A byte order mark wins, then the charset parameter of \a contentType, then the
 * encoding an XML declaration names; otherwise the body is taken as UTF-8.
 *
 * Data counts as binary when a NUL byte occurs early, unless it is in a wide
 * encoding, or when it is not valid UTF-8 and nothing says it is text. If
 * something does, through a text, JSON, or XML media type or a charset Qt does
 * not know, the bytes are read as Latin-1: that shows most legacy text readably
 * and never hides a body from the user.
 * \param body The raw body.
 * \param contentType The value of the Content-Type header; may be empty.
 * \return The text and its kind.
 */
DecodedBody decodeBody(const QByteArray &body, const QString &contentType)
{
    const QString mediaType = mediaTypeOf(contentType);
    QString charset = charsetOf(contentType);
    if (charset.isEmpty())
        charset = xmlDeclaredEncoding(body);

    const QByteArray charsetName = charset.toLatin1();
    QStringDecoder declared(charsetName.constData());
    const bool hasCharset = !charset.isEmpty() && declared.isValid();

    const std::optional<QStringConverter::Encoding> mark = QStringConverter::encodingForData(body);
    const bool wideMark = mark && *mark != QStringConverter::Utf8;
    const bool wide = wideMark || (hasCharset && isWideCharset(charset));

    DecodedBody decoded;
    if (!wide && QByteArrayView(body).first(qMin(body.size(), kBinaryProbeBytes)).contains('\0')) {
        decoded.kind = BodyKind::Binary;
        return decoded;
    }

    QStringDecoder decoder = wideMark ? QStringDecoder(*mark)
                             : hasCharset ? std::move(declared)
                                          : QStringDecoder(QStringDecoder::Utf8);
    QString text = decoder.decode(body);
    if (decoder.hasError() && !hasCharset) {
        const bool isText = !charset.isEmpty() || mediaType.startsWith(QLatin1StringView("text/"))
                            || mediaType.contains(QLatin1StringView("json"))
                            || mediaType.contains(QLatin1StringView("xml"));
        if (!isText) {
            decoded.kind = BodyKind::Binary;
            return decoded;
        }
        text = QString::fromLatin1(body);
    }

    decoded.kind = detectKind(text, mediaType);
    decoded.text = std::move(text);
    return decoded;
}

/*!
 * \brief Returns \a text indented.
 *
 * QJsonDocument only checks validity: writing it back would sort the keys, drop
 * duplicates, and turn large integers into doubles, which misrepresents what
 * the server sent. So this walks the original text and changes only whitespace
 * outside of strings. Deep nesting can multiply the size, so formatting gives up
 * beyond kMaxFormattedChars.
 * \param text The JSON text.
 * \return The indented text, or \a text when it is not valid JSON or gets too long.
 */
QString prettyJson(const QString &text)
{
    if (!isJson(text))
        return text;

    QString output;
    output.reserve(text.size() + text.size() / 4);
    const auto newline = [&output](int depth) {
        output += u'\n';
        output += QString(depth * kIndentWidth, u' ');
    };

    int depth = 0;
    const qsizetype size = text.size();
    for (qsizetype i = 0; i < size; ++i) {
        if (output.size() > kMaxFormattedChars)
            return text;

        const QChar c = text.at(i);
        switch (c.unicode()) {
        case u' ':
        case u'\t':
        case u'\r':
        case u'\n':
            break;
        case u'"': {
            output += c;
            for (++i; i < size; ++i) {
                output += text.at(i);
                if (text.at(i) == u'\\')
                    output += text.at(++i);
                else if (text.at(i) == u'"')
                    break;
            }
            break;
        }
        case u'{':
        case u'[': {
            qsizetype next = i + 1;
            while (next < size && text.at(next).isSpace())
                ++next;
            output += c;
            if (next < size && (text.at(next) == u'}' || text.at(next) == u']')) {
                output += text.at(next);
                i = next;
            } else {
                newline(++depth);
            }
            break;
        }
        case u'}':
        case u']':
            newline(--depth);
            output += c;
            break;
        case u',':
            output += c;
            newline(depth);
            break;
        case u':':
            output += QLatin1StringView(": ");
            break;
        default:
            output += c;
            break;
        }
    }
    return output;
}

/*!
 * \brief Returns \a text indented.
 * \param text The XML text.
 * \return The indented text, or \a text when it is not well-formed.
 */
QString prettyXml(const QString &text)
{
    return indentXml(text).value_or(text);
}
