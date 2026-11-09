// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/bodyhighlighter.h"

#include <QTextDocument>

#include "framework/bodyformatter.h"

namespace {
/*!
 * \internal
 * \brief Block state of a line that ends inside an XML comment.
 */
constexpr int kInsideComment = 1;

/*!
 * \internal
 * \brief Block state of a line that ends inside a CDATA section.
 */
constexpr int kInsideCdata = 2;

/*!
 * \internal
 * \brief Block state of a line that ends inside a tag, before its ">".
 */
constexpr int kInsideTag = 3;

/*!
 * \internal
 * \brief Returns the index after the string that opens at \a start in \a text.
 *
 * A backslash escapes the next character; an unterminated string ends with the
 * line.
 */
qsizetype endOfString(const QString &text, qsizetype start, QChar quote)
{
    qsizetype i = start + 1;
    while (i < text.size()) {
        if (text.at(i) == u'\\')
            i += 2;
        else if (text.at(i) == quote)
            return i + 1;
        else
            ++i;
    }
    return text.size();
}

/*!
 * \internal
 * \brief Returns whether \a c continues a JSON number or keyword.
 */
bool isTokenCharacter(QChar c)
{
    return c.isLetterOrNumber() || c == u'.' || c == u'+' || c == u'-';
}
} // namespace

/*!
 * \brief Creates a highlighter without a document.
 * \param parent The QObject parent.
 */
BodyHighlighter::BodyHighlighter(QObject *parent)
    : QSyntaxHighlighter(parent)
{
    updateFormats();
}

/*!
 * \brief Sets what to color.
 * \param kind A BodyKind value.
 */
void BodyHighlighter::setKind(int kind)
{
    if (m_kind == kind)
        return;
    m_kind = kind;
    rehighlight();
}

/*!
 * \brief Selects the palette for a dark background and recolors the document.
 *
 * Recoloring a document shown by a text item is quadratic in its lines: 200 KB
 * of formatted JSON takes half a minute. A view uses selectPalette() and sets
 * its text again, which is as fast as showing it the first time.
 * \param dark True for the dark palette.
 */
void BodyHighlighter::setDark(bool dark)
{
    if (m_dark == dark)
        return;
    selectPalette(dark);
    rehighlight();
}

/*!
 * \brief Selects the palette for text set afterwards.
 * \param dark True for the dark palette.
 */
void BodyHighlighter::selectPalette(bool dark)
{
    m_dark = dark;
    updateFormats();
}

/*!
 * \brief Colors the block \a text.
 *
 * Text and binary bodies stay plain, and so do very long documents, because
 * coloring happens on the GUI thread.
 * \param text The text of the block.
 */
void BodyHighlighter::highlightBlock(const QString &text)
{
    if (document()->characterCount() > kMaxHighlightedChars)
        return;
    if (m_kind == int(BodyKind::Json))
        highlightJson(text);
    else if (m_kind == int(BodyKind::Xml))
        highlightXml(text);
}

/*!
 * \internal
 * \brief Colors one line of JSON.
 *
 * A string followed by a colon is a key. Strings cannot span lines in JSON, so
 * no state is carried between blocks.
 */
void BodyHighlighter::highlightJson(const QString &text)
{
    const qsizetype size = text.size();
    qsizetype i = 0;
    while (i < size) {
        const QChar c = text.at(i);
        if (c == u'"') {
            const qsizetype end = endOfString(text, i, c);
            qsizetype next = end;
            while (next < size && text.at(next).isSpace())
                ++next;
            const bool isKey = next < size && text.at(next) == u':';
            setFormat(int(i), int(end - i), isKey ? m_keyFormat : m_stringFormat);
            i = end;
        } else if (c == u'-' || c.isDigit() || c.isLetter()) {
            qsizetype end = i + 1;
            while (end < size && isTokenCharacter(text.at(end)))
                ++end;
            setFormat(int(i), int(end - i), c.isLetter() ? m_keywordFormat : m_numberFormat);
            i = end;
        } else {
            ++i;
        }
    }
}

/*!
 * \internal
 * \brief Colors one line of XML.
 *
 * Comments, CDATA sections, and tags may span lines, which the block state
 * records. CDATA content is text, colored as a string; text between tags stays
 * plain.
 */
void BodyHighlighter::highlightXml(const QString &text)
{
    const qsizetype size = text.size();
    qsizetype i = 0;

    // Colors the rest of a section closed by \a close in \a format, from \a from;
    // returns where it ends, or -1 when it goes on in the next line with \a state.
    const auto section = [&](qsizetype from, QLatin1StringView close, const QTextCharFormat &format, int state) {
        const qsizetype found = text.indexOf(close, from);
        const qsizetype end = found < 0 ? size : found + close.size();
        setFormat(int(from), int(end - from), format);
        if (found < 0) {
            setCurrentBlockState(state);
            return qsizetype(-1);
        }
        return end;
    };

    setCurrentBlockState(0);
    switch (previousBlockState()) {
    case kInsideComment:
        i = section(0, QLatin1StringView("-->"), m_commentFormat, kInsideComment);
        break;
    case kInsideCdata:
        i = section(0, QLatin1StringView("]]>"), m_stringFormat, kInsideCdata);
        break;
    case kInsideTag:
        i = highlightXmlAttributes(text, 0);
        break;
    default:
        break;
    }
    if (i < 0)
        return;

    while (i < size) {
        if (text.at(i) != u'<') {
            ++i;
            continue;
        }

        const QStringView rest = QStringView(text).sliced(i);
        if (rest.startsWith(QLatin1StringView("<!--"))) {
            i = section(i, QLatin1StringView("-->"), m_commentFormat, kInsideComment);
            if (i < 0)
                return;
            continue;
        }
        if (rest.startsWith(QLatin1StringView("<![CDATA["))) {
            setFormat(int(i), 9, m_keywordFormat);
            i = section(i + 9, QLatin1StringView("]]>"), m_stringFormat, kInsideCdata);
            if (i < 0)
                return;
            continue;
        }

        // Tag name, including the opening bracket and a leading slash.
        qsizetype end = i + 1;
        while (end < size && !text.at(end).isSpace() && text.at(end) != u'>')
            ++end;
        setFormat(int(i), int(end - i), m_keywordFormat);
        i = highlightXmlAttributes(text, end);
        if (i < 0)
            return;
    }
}

/*!
 * \internal
 * \brief Colors the attributes of a tag in \a text from \a from up to and with its ">".
 *
 * A tag that goes on in the next line sets the block state to kInsideTag.
 * \param text The line.
 * \param from Where the attributes start.
 * \return The position after the ">", or -1 when the tag continues in the next line.
 */
qsizetype BodyHighlighter::highlightXmlAttributes(const QString &text, qsizetype from)
{
    const qsizetype size = text.size();
    qsizetype i = from;
    while (i < size && text.at(i) != u'>') {
        const QChar c = text.at(i);
        if (c == u'"' || c == u'\'') {
            const qsizetype stringEnd = endOfString(text, i, c);
            setFormat(int(i), int(stringEnd - i), m_stringFormat);
            i = stringEnd;
        } else if (c.isSpace() || c == u'=' || c == u'/' || c == u'?') {
            ++i;
        } else {
            qsizetype nameEnd = i + 1;
            while (nameEnd < size && !text.at(nameEnd).isSpace() && text.at(nameEnd) != u'='
                   && text.at(nameEnd) != u'>') {
                ++nameEnd;
            }
            setFormat(int(i), int(nameEnd - i), m_keyFormat);
            i = nameEnd;
        }
    }
    if (i >= size) {
        setCurrentBlockState(kInsideTag);
        return -1;
    }
    setFormat(int(i), 1, m_keywordFormat);
    return i + 1;
}

/*!
 * \internal
 * \brief Applies the colors of the active palette to the formats.
 */
void BodyHighlighter::updateFormats()
{
    m_keyFormat.setForeground(QColor(m_dark ? 0x9CDCFE : 0x0451A5));
    m_stringFormat.setForeground(QColor(m_dark ? 0xCE9178 : 0xA31515));
    m_numberFormat.setForeground(QColor(m_dark ? 0xB5CEA8 : 0x098658));
    m_keywordFormat.setForeground(QColor(m_dark ? 0x569CD6 : 0x0000FF));
    m_commentFormat.setForeground(QColor(m_dark ? 0x6A9955 : 0x008000));
}
