// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtTest>

#include "framework/bodyformatter.h"
#include "qmlapi/bodyhighlighter.h"

/*!
 * \internal
 * \brief Returns the foreground color at \a position of \a document; invalid when plain.
 */
static QColor colorAt(const QTextDocument &document, int position)
{
    const QTextBlock block = document.findBlock(position);
    const int offset = position - block.position();
    const QList<QTextLayout::FormatRange> formats = block.layout()->formats();
    for (const QTextLayout::FormatRange &range : formats) {
        if (offset >= range.start && offset < range.start + range.length)
            return range.format.foreground().color();
    }
    return {};
}

/*! Verifies the coloring of response bodies. */
class BodyHighlighterTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies keys, strings, numbers, and keywords in JSON. */
    void colorsJsonTokens();

    /*! Verifies that a string after a colon is a value, not a key. */
    void jsonStringWithEscapedQuoteStaysOneToken();

    /*! Verifies tags, attributes, values, and comments in XML. */
    void colorsXmlTokens();

    /*! Verifies a comment running over several lines. */
    void colorsMultilineXmlComment();

    /*! Verifies that CDATA reads as text and attributes keep their colors on the next lines of a tag. */
    void colorsCdataAndMultilineTags();

    /*! Verifies that text and binary bodies stay plain. */
    void textKindHasNoFormats_data();
    /*! Runs one plain kind case. */
    void textKindHasNoFormats();

    /*! Verifies that very long documents are left alone. */
    void highlightingStopsAboveLimit();

    /*! Verifies the two palettes. */
    void darkAndLightUseTheirPalette();
};

void BodyHighlighterTest::colorsJsonTokens()
{
    QTextDocument document(QStringLiteral("{\"key\": \"text\", \"n\": -12.5, \"b\": true, \"c\": null}"));
    BodyHighlighter highlighter;
    highlighter.setDark(true);
    highlighter.setKind(int(BodyKind::Json));
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    const QString text = document.toPlainText();
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("key"))), QColor(0x9C, 0xDC, 0xFE));
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("text"))), QColor(0xCE, 0x91, 0x78));
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("-12.5"))), QColor(0xB5, 0xCE, 0xA8));
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("true"))), QColor(0x56, 0x9C, 0xD6));
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("null"))), QColor(0x56, 0x9C, 0xD6));
    QVERIFY(!colorAt(document, 0).isValid());
}

void BodyHighlighterTest::jsonStringWithEscapedQuoteStaysOneToken()
{
    QTextDocument document(QStringLiteral("[\"a\\\"true\"]"));
    BodyHighlighter highlighter;
    highlighter.setDark(true);
    highlighter.setKind(int(BodyKind::Json));
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    const int position = document.toPlainText().indexOf(QStringLiteral("true"));
    QCOMPARE(colorAt(document, position), QColor(0xCE, 0x91, 0x78));
}

void BodyHighlighterTest::colorsXmlTokens()
{
    QTextDocument document(QStringLiteral("<a x=\"1\">text<!-- note --></a>"));
    BodyHighlighter highlighter;
    highlighter.setDark(true);
    highlighter.setKind(int(BodyKind::Xml));
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    const QString text = document.toPlainText();
    QCOMPARE(colorAt(document, 1), QColor(0x56, 0x9C, 0xD6));
    QCOMPARE(colorAt(document, text.indexOf(u'x')), QColor(0x9C, 0xDC, 0xFE));
    QCOMPARE(colorAt(document, text.indexOf(u'1')), QColor(0xCE, 0x91, 0x78));
    QVERIFY(!colorAt(document, text.indexOf(QStringLiteral("text"))).isValid());
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("note"))), QColor(0x6A, 0x99, 0x55));
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("</a")) + 2), QColor(0x56, 0x9C, 0xD6));
}

void BodyHighlighterTest::colorsMultilineXmlComment()
{
    QTextDocument document(QStringLiteral("<!-- one\ntwo\nthree --><a/>"));
    BodyHighlighter highlighter;
    highlighter.setDark(true);
    highlighter.setKind(int(BodyKind::Xml));
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    const QString text = document.toPlainText();
    const QColor comment(0x6A, 0x99, 0x55);
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("two"))), comment);
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("three"))), comment);
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("<a")) + 1), QColor(0x56, 0x9C, 0xD6));
}

void BodyHighlighterTest::colorsCdataAndMultilineTags()
{
    QTextDocument document(QStringLiteral("<a\n  x=\"1\"\n  y='2'>\n<![CDATA[ <b> one\ntwo ]]><c/></a>"));
    BodyHighlighter highlighter;
    highlighter.setDark(true);
    highlighter.setKind(int(BodyKind::Xml));
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    const QString text = document.toPlainText();
    const QColor keyword(0x56, 0x9C, 0xD6);
    const QColor key(0x9C, 0xDC, 0xFE);
    const QColor string(0xCE, 0x91, 0x78);
    QCOMPARE(colorAt(document, text.indexOf(u'x')), key);
    QCOMPARE(colorAt(document, text.indexOf(u'1')), string);
    QCOMPARE(colorAt(document, text.indexOf(u'y')), key);
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("'>")) + 1), keyword);
    // Inside CDATA is text, not markup, also on the next line.
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("<b>")) + 1), string);
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("two"))), string);
    QCOMPARE(colorAt(document, text.indexOf(QStringLiteral("<c")) + 1), keyword);
}

void BodyHighlighterTest::textKindHasNoFormats_data()
{
    QTest::addColumn<BodyKind>("kind");

    QTest::newRow("text") << BodyKind::Text;
    QTest::newRow("binary") << BodyKind::Binary;
}

void BodyHighlighterTest::textKindHasNoFormats()
{
    QFETCH(BodyKind, kind);

    QTextDocument document(QStringLiteral("{\"a\": 1}"));
    BodyHighlighter highlighter;
    highlighter.setKind(int(kind));
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    QVERIFY(document.firstBlock().layout()->formats().isEmpty());
}

void BodyHighlighterTest::highlightingStopsAboveLimit()
{
    QTextDocument document;
    document.setPlainText(QStringLiteral("[1]\n") + QString(BodyHighlighter::kMaxHighlightedChars, u' '));
    BodyHighlighter highlighter;
    highlighter.setKind(int(BodyKind::Json));
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    QVERIFY(document.firstBlock().layout()->formats().isEmpty());
}

void BodyHighlighterTest::darkAndLightUseTheirPalette()
{
    QTextDocument document(QStringLiteral("[\"s\"]"));
    BodyHighlighter highlighter;
    highlighter.setKind(int(BodyKind::Json));
    highlighter.setDocument(&document);
    highlighter.rehighlight();
    QCOMPARE(colorAt(document, 2), QColor(0xA3, 0x15, 0x15));

    highlighter.setDark(true);
    QCOMPARE(colorAt(document, 2), QColor(0xCE, 0x91, 0x78));
}

QTEST_MAIN(BodyHighlighterTest)

#include "tst_bodyhighlighter.moc"
