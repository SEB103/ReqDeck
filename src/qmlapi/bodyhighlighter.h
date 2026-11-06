// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BODYHIGHLIGHTER_H
#define BODYHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/** Colors JSON and XML in a read-only body view, in the colors of VS Code. */
class BodyHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BodyHighlighter)

public:
    /** Longest document that is colored; beyond this, editing the view gets slow. */
    static constexpr qsizetype kMaxHighlightedChars = 262144;

    /** Creates a highlighter without a document. */
    explicit BodyHighlighter(QObject *parent = nullptr);

    /** Sets what to color, as a BodyKind value. */
    void setKind(int kind);

    /** Selects the palette for a dark background and recolors the document. */
    void setDark(bool dark);

    /** Selects the palette for text set afterwards; existing text keeps its colors. */
    void selectPalette(bool dark);

protected:
    /** Colors the block \a text. */
    void highlightBlock(const QString &text) override;

private:
    /** Colors one line of JSON. */
    void highlightJson(const QString &text);

    /** Colors one line of XML. */
    void highlightXml(const QString &text);

    /** Applies the colors of the active palette to the formats. */
    void updateFormats();

    /** What to color, as a BodyKind value. */
    int m_kind {0};

    /** Whether the dark palette is active. */
    bool m_dark {false};

    /** Format of JSON keys and XML attribute names. */
    QTextCharFormat m_keyFormat;

    /** Format of strings and attribute values. */
    QTextCharFormat m_stringFormat;

    /** Format of numbers. */
    QTextCharFormat m_numberFormat;

    /** Format of JSON keywords and XML tags. */
    QTextCharFormat m_keywordFormat;

    /** Format of XML comments. */
    QTextCharFormat m_commentFormat;
};

#endif // BODYHIGHLIGHTER_H
