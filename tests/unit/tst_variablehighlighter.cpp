// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QtTest>

#include "core/http/variableresolver.h"
#include "qmlapi/variablehighlighter.h"

/*!
 * \internal
 * \brief Returns the format ranges of the first line of \a document.
 */
static QList<QTextLayout::FormatRange> formatsOf(const QTextDocument &document)
{
    return document.firstBlock().layout()->formats();
}

/*!
 * \internal
 * \brief Returns a resolver knowing only the variable \a name.
 */
static VariableResolver resolverWith(const QString &name)
{
    return VariableResolver(QHash<QString, QString> {{name, QStringLiteral("value")}});
}

/*! Verifies the coloring of variable references. */
class VariableHighlighterTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that only an unresolved reference is underlined, in its own color. */
    void knownAndUnresolvedDiffer();

    /*! Verifies that text without references stays plain. */
    void plainTextHasNoFormats();

    /*! Verifies that new variables recolor the document. */
    void settingVariablesRecolors();

    /*! Verifies that the dark palette has its own colors. */
    void settingDarkChangesColors();

    /*! Verifies that a response() template has its own color and is not marked unresolved. */
    void responseTemplateHasOwnColor();
};

void VariableHighlighterTest::knownAndUnresolvedDiffer()
{
    QTextDocument document(QStringLiteral("{{a}}{{b}}"));
    VariableHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.setVariables(resolverWith(QStringLiteral("a")));

    const QList<QTextLayout::FormatRange> formats = formatsOf(document);

    QCOMPARE(formats.size(), 2);
    QCOMPARE(formats.at(0).start, 0);
    QCOMPARE(formats.at(1).start, 5);
    QVERIFY(formats.at(0).format.foreground() != formats.at(1).format.foreground());
    QVERIFY(!formats.at(0).format.fontUnderline());
    QVERIFY(formats.at(1).format.fontUnderline());
}

void VariableHighlighterTest::responseTemplateHasOwnColor()
{
    QTextDocument document(QStringLiteral(R"({{a}}{{response("A", "$")}}{{b}})"));
    VariableHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.setVariables(resolverWith(QStringLiteral("a")));

    const QList<QTextLayout::FormatRange> formats = formatsOf(document);
    QCOMPARE(formats.size(), 3);
    QCOMPARE(formats.at(1).start, 5);
    QVERIFY(!formats.at(1).format.fontUnderline());
    QVERIFY(formats.at(1).format.foreground() != formats.at(0).format.foreground());
    QVERIFY(formats.at(1).format.foreground() != formats.at(2).format.foreground());
}

void VariableHighlighterTest::plainTextHasNoFormats()
{
    QTextDocument document(QStringLiteral("http://example.com/{not}"));
    VariableHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.rehighlight();

    QVERIFY(formatsOf(document).isEmpty());
}

void VariableHighlighterTest::settingVariablesRecolors()
{
    QTextDocument document(QStringLiteral("{{b}}"));
    VariableHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.rehighlight();
    QVERIFY(formatsOf(document).at(0).format.fontUnderline());

    highlighter.setVariables(resolverWith(QStringLiteral("b")));

    QVERIFY(!formatsOf(document).at(0).format.fontUnderline());
}

void VariableHighlighterTest::settingDarkChangesColors()
{
    QTextDocument document(QStringLiteral("{{b}}"));
    VariableHighlighter highlighter;
    highlighter.setDocument(&document);
    highlighter.rehighlight();
    const QColor light = formatsOf(document).at(0).format.foreground().color();

    highlighter.setDark(true);

    QVERIFY(formatsOf(document).at(0).format.foreground().color() != light);
}

QTEST_MAIN(VariableHighlighterTest)

#include "tst_variablehighlighter.moc"
