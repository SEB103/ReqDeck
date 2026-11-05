// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "framework/bodyformatter.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QLatin1StringView>
#include <QStringDecoder>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <optional>

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
        const QString part = parts.at(i).trimmed();
        if (!part.startsWith(QLatin1StringView("charset="), Qt::CaseInsensitive))
            continue;
        QString charset = part.mid(8).trimmed();
        charset.remove(u'"');
        return charset;
    }
    return {};
}

/*!
 * \internal
 * \brief Returns whether \a text is valid JSON.
 */
bool isJson(const QString &text)
{
    QJsonParseError error;
    QJsonDocument::fromJson(text.toUtf8(), &error);
    return error.error == QJsonParseError::NoError;
}

/*!
 * \internal
 * \brief Writes \a text as indented XML; nullopt when it is not well-formed.
 *
 * The reader also rejects trailing garbage and unbalanced tags, so the same
 * function decides whether a body is XML at all.
 */
std::optional<QString> indentXml(const QString &text)
{
    QString output;
    QXmlStreamWriter writer(&output);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(kIndentWidth);

    QXmlStreamReader reader(text);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.hasError())
            return std::nullopt;
        if (reader.isWhitespace())
            continue;
        // Writing this token would add a declaration the document did not have.
        if (reader.isStartDocument() && reader.documentVersion().isEmpty())
            continue;
        writer.writeCurrentToken(reader);
    }
    return output.trimmed();
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
 * The charset parameter of \a contentType is honored; a missing or unknown one
 * means UTF-8. Data counts as binary when a NUL byte occurs early, or when it is
 * not valid UTF-8 although no charset was declared. With a declared charset,
 * invalid bytes are shown as replacement characters instead: the server said it
 * is text.
 * \param body The raw body.
 * \param contentType The value of the Content-Type header; may be empty.
 * \return The text and its kind.
 */
DecodedBody decodeBody(const QByteArray &body, const QString &contentType)
{
    DecodedBody decoded;
    if (QByteArrayView(body).first(qMin(body.size(), kBinaryProbeBytes)).contains('\0')) {
        decoded.kind = BodyKind::Binary;
        return decoded;
    }

    const QByteArray charset = charsetOf(contentType).toLatin1();
    QStringDecoder declared(charset.constData());
    const bool hasCharset = !charset.isEmpty() && declared.isValid();

    QStringDecoder utf8(QStringDecoder::Utf8);
    QStringDecoder &decoder = hasCharset ? declared : utf8;
    QString text = decoder.decode(body);
    if (decoder.hasError() && !hasCharset) {
        decoded.kind = BodyKind::Binary;
        return decoded;
    }

    decoded.kind = detectKind(text, mediaTypeOf(contentType));
    decoded.text = std::move(text);
    return decoded;
}

/*!
 * \brief Returns \a text indented.
 *
 * QJsonDocument only checks validity: writing it back would sort the keys, drop
 * duplicates, and turn large integers into doubles, which misrepresents what
 * the server sent. So this walks the original text and changes only whitespace
 * outside of strings.
 * \param text The JSON text.
 * \return The indented text, or \a text when it is not valid JSON.
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
