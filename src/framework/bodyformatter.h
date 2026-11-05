// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BODYFORMATTER_H
#define BODYFORMATTER_H

#include <QByteArray>
#include <QString>

/** What a body contains, which decides how it can be shown. */
enum class BodyKind {
    Text,
    Json,
    Xml,
    Binary
};

/** Most characters of a body shown at once; the text view gets slow beyond this. */
inline constexpr qsizetype kMaxDisplayChars = 1048576;

/** A body decoded for display. */
struct DecodedBody
{
    /** The decoded text; empty for binary data. */
    QString text;
    /** What the body contains. */
    BodyKind kind {BodyKind::Text};
};

/** Decodes \a body by the charset of \a contentType and detects what it contains. */
DecodedBody decodeBody(const QByteArray &body, const QString &contentType);

/** Returns \a text indented, or unchanged when it is not valid JSON. */
QString prettyJson(const QString &text);

/** Returns \a text indented, or unchanged when it is not well-formed XML. */
QString prettyXml(const QString &text);

#endif // BODYFORMATTER_H
