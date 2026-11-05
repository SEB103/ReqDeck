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
 * \brief Selects the palette for a dark background.
 * \param dark True for the dark palette.
 */
void BodyHighlighter::setDark(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    updateFormats();
    rehighlight();
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
 * Comments may span lines, which the block state records. Tags are colored
 * within the line; text between tags stays plain.
 */
void BodyHighlighter::highlightXml(const QString &text)
{
    const qsizetype size = text.size();
    qsizetype i = 0;

    if (previousBlockState() == kInsideComment) {
        const qsizetype close = text.indexOf(QLatin1StringView("-->"));
        const qsizetype end = close < 0 ? size : close + 3;
        setFormat(0, int(end), m_commentFormat);
        if (close < 0) {
            setCurrentBlockState(kInsideComment);
            return;
        }
        i = end;
    }
    setCurrentBlockState(0);

    while (i < size) {
        if (text.at(i) != u'<') {
            ++i;
            continue;
        }

        if (QStringView(text).sliced(i).startsWith(QLatin1StringView("<!--"))) {
            const qsizetype close = text.indexOf(QLatin1StringView("-->"), i + 4);
            const qsizetype end = close < 0 ? size : close + 3;
            setFormat(int(i), int(end - i), m_commentFormat);
            if (close < 0) {
                setCurrentBlockState(kInsideComment);
                return;
            }
            i = end;
            continue;
        }

        // Tag name, including the opening bracket and a leading slash.
        qsizetype end = i + 1;
        while (end < size && !text.at(end).isSpace() && text.at(end) != u'>')
            ++end;
        setFormat(int(i), int(end - i), m_keywordFormat);
        i = end;

        // Attributes up to the end of the tag.
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
        if (i < size) {
            setFormat(int(i), 1, m_keywordFormat);
            ++i;
        }
    }
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
