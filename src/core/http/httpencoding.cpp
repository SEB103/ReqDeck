// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/httpencoding.h"

#include <QMimeDatabase>
#include <QUrl>

/*!
 * \brief Encodes \a pairs as an application/x-www-form-urlencoded style string.
 *
 * Disabled pairs and pairs with an empty key are skipped. Every other character
 * than the RFC 3986 unreserved set is percent-encoded from UTF-8, so '&', '=',
 * '+', '#', and spaces inside keys or values cannot break the pair structure.
 * \param pairs The key/value rows to encode.
 * \return The encoded pairs, or an empty array when no pair qualifies.
 */
QByteArray encodeKeyValuePairs(const QList<KeyValue> &pairs)
{
    QByteArray encoded;
    for (const KeyValue &pair : pairs) {
        if (!pair.enabled || pair.key.isEmpty())
            continue;
        if (!encoded.isEmpty())
            encoded += '&';
        encoded += QUrl::toPercentEncoding(pair.key);
        encoded += '=';
        encoded += QUrl::toPercentEncoding(pair.value);
    }
    return encoded;
}

/*!
 * \brief Returns the default Content-Type of a body.
 *
 * Text bodies declare UTF-8 explicitly because RequestBuilder encodes raw
 * text as UTF-8.
 * \param mode The body composition mode.
 * \param rawType The raw body flavor; used only for BodyMode::Raw.
 * \return The header value, or an empty array for BodyMode::None.
 */
QByteArray defaultContentType(BodyMode mode, RawBodyType rawType)
{
    switch (mode) {
    case BodyMode::None:
        return {};
    case BodyMode::FormUrlEncoded:
        return QByteArrayLiteral("application/x-www-form-urlencoded");
    case BodyMode::FormData:
        // RequestBuilder adds the boundary.
        return QByteArrayLiteral("multipart/form-data");
    case BodyMode::File:
        // RequestBuilder picks one from the file name.
        return {};
    case BodyMode::Raw:
        break;
    }
    switch (rawType) {
    case RawBodyType::Json:
        return QByteArrayLiteral("application/json");
    case RawBodyType::Xml:
        return QByteArrayLiteral("application/xml");
    case RawBodyType::Text:
        break;
    }
    return QByteArrayLiteral("text/plain; charset=utf-8");
}

/*!
 * \brief Returns the Content-Type for the file at \a path, judged by its name only.
 *
 * The content is not read, so a file that is not there yet still gets a type.
 * \param path The path or file name.
 * \return For example "image/png"; application/octet-stream for unknown names.
 */
QByteArray contentTypeForFile(const QString &path)
{
    const QMimeType type = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    return type.isValid() && !type.isDefault() ? type.name().toLatin1() : QByteArrayLiteral("application/octet-stream");
}
