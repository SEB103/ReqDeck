// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "framework/bodyformatter.h"

/*! Verifies decoding and formatting of response bodies. */
class BodyFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that the media type decides the kind. */
    void detectsKindFromContentType_data();
    /*! Runs one media type case. */
    void detectsKindFromContentType();

    /*! Verifies detection by content when the media type says nothing. */
    void detectsKindFromContent_data();
    /*! Runs one content case. */
    void detectsKindFromContent();

    /*! Verifies that binary data is recognized. */
    void binaryBodyIsNotShownAsText_data();
    /*! Runs one binary case. */
    void binaryBodyIsNotShownAsText();

    /*! Verifies that a declared charset is honored. */
    void decodesDeclaredCharset();

    /*! Verifies the fallback for a charset Qt does not know. */
    void unknownCharsetFallsBackToUtf8();

    /*! Verifies that invalid bytes are shown when UTF-8 was declared. */
    void declaredCharsetIsNeverBinaryForInvalidBytes();

    /*! Verifies that a byte order mark does not reach the text. */
    void byteOrderMarkIsDropped();

    /*! Verifies an empty body. */
    void emptyBodyIsText();

    /*! Verifies that wide encodings are text although they contain NUL bytes. */
    void decodesWideEncodings_data();
    /*! Runs one wide encoding case. */
    void decodesWideEncodings();

    /*! Verifies that bytes that are not UTF-8 stay readable in text types. */
    void textTypesAreNeverBinaryForInvalidBytes_data();
    /*! Runs one invalid bytes case. */
    void textTypesAreNeverBinaryForInvalidBytes();

    /*! Verifies spaces around the equals sign of the charset parameter. */
    void charsetParameterMayContainSpaces();

    /*! Verifies the encoding named in an XML declaration. */
    void xmlDeclarationEncodingIsHonored();

    /*! Verifies the layout of pretty JSON. */
    void prettyJsonIndents();

    /*! Verifies that Pretty keeps what the server sent. */
    void prettyJsonKeepsKeyOrderAndNumbers();

    /*! Verifies that brackets inside strings are not structure. */
    void prettyJsonIgnoresBracketsInStrings();

    /*! Verifies that a number beyond the range of a double does not stop the formatting. */
    void prettyJsonKeepsHugeNumbers();

    /*! Verifies empty objects and arrays. */
    void prettyJsonHandlesEmptyContainers();

    /*! Verifies that text that is no JSON stays as it is. */
    void prettyJsonReturnsInvalidInputUnchanged();

    /*! Verifies the layout of pretty XML. */
    void prettyXmlIndents();

    /*! Verifies that text that is no XML stays as it is. */
    void prettyXmlReturnsInvalidInputUnchanged();

    /*! Verifies that formatting changes whitespace between elements only. */
    void prettyXmlKeepsTheDocument_data();
    /*! Runs one fidelity case. */
    void prettyXmlKeepsTheDocument();

    /*! Verifies the indentation of comments and nested elements. */
    void prettyXmlIndentsStructure();

    /*! Verifies that deep nesting cannot blow up the formatted text. */
    void prettyJsonGivesUpOnHugeOutput();
};

void BodyFormatterTest::detectsKindFromContentType_data()
{
    QTest::addColumn<QString>("contentType");
    QTest::addColumn<BodyKind>("kind");

    QTest::newRow("json") << QStringLiteral("application/json") << BodyKind::Json;
    QTest::newRow("json-charset") << QStringLiteral("application/json; charset=utf-8") << BodyKind::Json;
    QTest::newRow("problem") << QStringLiteral("application/problem+json") << BodyKind::Json;
    QTest::newRow("upper") << QStringLiteral("Application/JSON") << BodyKind::Json;
    QTest::newRow("xml") << QStringLiteral("application/xml") << BodyKind::Xml;
    QTest::newRow("text-xml") << QStringLiteral("text/xml") << BodyKind::Xml;
    QTest::newRow("soap") << QStringLiteral("application/soap+xml") << BodyKind::Xml;
    QTest::newRow("html") << QStringLiteral("text/html") << BodyKind::Text;
}

void BodyFormatterTest::detectsKindFromContentType()
{
    QFETCH(QString, contentType);
    QFETCH(BodyKind, kind);

    QCOMPARE(decodeBody("anything", contentType).kind, kind);
}

void BodyFormatterTest::detectsKindFromContent_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<BodyKind>("kind");

    QTest::newRow("object") << QByteArray("  {\"a\":1}") << BodyKind::Json;
    QTest::newRow("array") << QByteArray("[1,2]") << BodyKind::Json;
    QTest::newRow("broken-json") << QByteArray("{oops") << BodyKind::Text;
    QTest::newRow("xml") << QByteArray("<a><b/></a>") << BodyKind::Xml;
    QTest::newRow("broken-xml") << QByteArray("<a><b></a>") << BodyKind::Text;
    QTest::newRow("words") << QByteArray("hello") << BodyKind::Text;
}

void BodyFormatterTest::detectsKindFromContent()
{
    QFETCH(QByteArray, body);
    QFETCH(BodyKind, kind);

    QCOMPARE(decodeBody(body, QString()).kind, kind);
    QCOMPARE(decodeBody(body, QStringLiteral("text/plain")).kind, kind);
}

void BodyFormatterTest::binaryBodyIsNotShownAsText_data()
{
    QTest::addColumn<QByteArray>("body");

    QTest::newRow("nul") << QByteArray("abc\0def", 7);
    QTest::newRow("png") << QByteArray::fromHex("89504E470D0A1A0A0000000D");
    QTest::newRow("invalid-utf8") << QByteArray::fromHex("6162FFFE63");
}

void BodyFormatterTest::binaryBodyIsNotShownAsText()
{
    QFETCH(QByteArray, body);

    const DecodedBody decoded = decodeBody(body, QString());
    QCOMPARE(decoded.kind, BodyKind::Binary);
    QVERIFY(decoded.text.isEmpty());
}

void BodyFormatterTest::decodesDeclaredCharset()
{
    const DecodedBody decoded = decodeBody(QByteArray::fromHex("4DFC"),
                                           QStringLiteral("text/plain; charset=iso-8859-1"));
    QCOMPARE(decoded.text, QStringLiteral("Mü"));
    QCOMPARE(decoded.kind, BodyKind::Text);

    const DecodedBody quoted = decodeBody(QByteArray::fromHex("4DFC"),
                                          QStringLiteral("text/plain; Charset=\"ISO-8859-1\""));
    QCOMPARE(quoted.text, QStringLiteral("Mü"));
}

void BodyFormatterTest::unknownCharsetFallsBackToUtf8()
{
    const DecodedBody decoded = decodeBody("M\xC3\xBC", QStringLiteral("text/plain; charset=nope"));
    QCOMPARE(decoded.text, QStringLiteral("Mü"));
}

void BodyFormatterTest::declaredCharsetIsNeverBinaryForInvalidBytes()
{
    const DecodedBody decoded = decodeBody(QByteArray::fromHex("61FF62"),
                                           QStringLiteral("text/plain; charset=utf-8"));
    QCOMPARE(decoded.kind, BodyKind::Text);
    QVERIFY(decoded.text.startsWith(u'a'));
}

void BodyFormatterTest::byteOrderMarkIsDropped()
{
    const DecodedBody decoded = decodeBody(QByteArray::fromHex("EFBBBF7B7D"), QString());
    QCOMPARE(decoded.text, QStringLiteral("{}"));
    QCOMPARE(decoded.kind, BodyKind::Json);
}

void BodyFormatterTest::emptyBodyIsText()
{
    const DecodedBody decoded = decodeBody(QByteArray(), QString());
    QCOMPARE(decoded.kind, BodyKind::Text);
    QVERIFY(decoded.text.isEmpty());
}

void BodyFormatterTest::decodesWideEncodings_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("contentType");

    const QByteArray le = QByteArray::fromHex("4D00FC00");
    const QByteArray be = QByteArray::fromHex("004D00FC");
    QTest::newRow("bom-le") << QByteArray::fromHex("FFFE") + le << QString();
    QTest::newRow("bom-be") << QByteArray::fromHex("FEFF") + be << QString();
    QTest::newRow("declared-le") << le << QStringLiteral("text/plain; charset=utf-16le");
    QTest::newRow("declared-be") << be << QStringLiteral("text/plain; charset=UTF-16BE");
    QTest::newRow("declared-bom") << QByteArray::fromHex("FFFE") + le
                                  << QStringLiteral("text/plain; charset=utf-16");
    QTest::newRow("utf-32") << QByteArray::fromHex("4D000000FC000000")
                            << QStringLiteral("text/plain; charset=utf-32le");
}

void BodyFormatterTest::decodesWideEncodings()
{
    QFETCH(QByteArray, body);
    QFETCH(QString, contentType);

    const DecodedBody decoded = decodeBody(body, contentType);
    QCOMPARE(decoded.kind, BodyKind::Text);
    QCOMPARE(decoded.text, QStringLiteral("M\u00FC"));
}

void BodyFormatterTest::textTypesAreNeverBinaryForInvalidBytes_data()
{
    QTest::addColumn<QString>("contentType");

    QTest::newRow("text") << QStringLiteral("text/plain");
    QTest::newRow("unknown-charset") << QStringLiteral("application/x-thing; charset=bogus");
    QTest::newRow("json") << QStringLiteral("application/json");
    QTest::newRow("xml") << QStringLiteral("application/xml");
}

void BodyFormatterTest::textTypesAreNeverBinaryForInvalidBytes()
{
    QFETCH(QString, contentType);

    const DecodedBody decoded = decodeBody(QByteArray::fromHex("4DFC"), contentType);
    QVERIFY(decoded.kind != BodyKind::Binary);
    QCOMPARE(decoded.text, QStringLiteral("M\u00FC"));
}

void BodyFormatterTest::charsetParameterMayContainSpaces()
{
    const DecodedBody decoded = decodeBody(QByteArray::fromHex("4DFC"),
                                           QStringLiteral("text/plain; charset = iso-8859-1"));
    QCOMPARE(decoded.text, QStringLiteral("M\u00FC"));
}

void BodyFormatterTest::xmlDeclarationEncodingIsHonored()
{
    const QByteArray body = "<?xml version=\"1.0\" encoding=\"windows-1251\"?><a>"
                            + QByteArray::fromHex("C0") + "</a>";
    const DecodedBody decoded = decodeBody(body, QStringLiteral("text/xml"));
    QCOMPARE(decoded.kind, BodyKind::Xml);
    QVERIFY(decoded.text.contains(QStringLiteral("<a>\u0410</a>")));
}

void BodyFormatterTest::prettyJsonIndents()
{
    QCOMPARE(prettyJson(QStringLiteral("{\"a\":[1,2],\"b\":{\"c\":true}}")),
             QStringLiteral("{\n  \"a\": [\n    1,\n    2\n  ],\n  \"b\": {\n    \"c\": true\n  }\n}"));
}

void BodyFormatterTest::prettyJsonKeepsKeyOrderAndNumbers()
{
    const QString text = prettyJson(
        QStringLiteral("{\"z\":1,\"a\":12345678901234567890,\"z\":2,\"e\":1.50e+3,\"s\":\"\\u00fc\\n\"}"));
    QCOMPARE(text,
             QStringLiteral("{\n  \"z\": 1,\n  \"a\": 12345678901234567890,\n  \"z\": 2,\n"
                            "  \"e\": 1.50e+3,\n  \"s\": \"\\u00fc\\n\"\n}"));
}

void BodyFormatterTest::prettyJsonKeepsHugeNumbers()
{
    QCOMPARE(prettyJson(QStringLiteral("[1e400,-2E-999]")), QStringLiteral("[\n  1e400,\n  -2E-999\n]"));
    // Still no JSON: a broken number, and digits inside a string are left alone.
    QCOMPARE(prettyJson(QStringLiteral("[1e]")), QStringLiteral("[1e]"));
    QCOMPARE(prettyJson(QStringLiteral("[\"1e400\",01]")), QStringLiteral("[\"1e400\",01]"));
}

void BodyFormatterTest::prettyJsonIgnoresBracketsInStrings()
{
    QCOMPARE(prettyJson(QStringLiteral("[\"a\\\"{,[\"]")),
             QStringLiteral("[\n  \"a\\\"{,[\"\n]"));
}

void BodyFormatterTest::prettyJsonHandlesEmptyContainers()
{
    QCOMPARE(prettyJson(QStringLiteral("{\"a\":{},\"b\":[ ]}")),
             QStringLiteral("{\n  \"a\": {},\n  \"b\": []\n}"));
}

void BodyFormatterTest::prettyJsonReturnsInvalidInputUnchanged()
{
    const QString text = QStringLiteral("{oops");
    QCOMPARE(prettyJson(text), text);
}

void BodyFormatterTest::prettyXmlIndents()
{
    QCOMPARE(prettyXml(QStringLiteral("<a x=\"1\"><b>t</b><c/></a>")),
             QStringLiteral("<a x=\"1\">\n  <b>t</b>\n  <c/>\n</a>"));
}

void BodyFormatterTest::prettyXmlReturnsInvalidInputUnchanged()
{
    const QString text = QStringLiteral("<a><b></a>");
    QCOMPARE(prettyXml(text), text);
}

void BodyFormatterTest::prettyXmlKeepsTheDocument_data()
{
    QTest::addColumn<QString>("text");

    QTest::newRow("declaration")
        << QStringLiteral("<?xml version=\"1.0\" encoding=\"ISO-8859-1\" standalone=\"yes\"?>\n<a/>");
    QTest::newRow("only-whitespace") << QStringLiteral("<a> </a>");
    QTest::newRow("preserve")
        << QStringLiteral("<a xml:space=\"preserve\"> <b/>  <c/> </a>");
    QTest::newRow("prefixes")
        << QStringLiteral("<p:a xmlns:p=\"u\" xmlns:q=\"u\">\n  <p:b>1</p:b>\n</p:a>");
    QTest::newRow("attlist")
        << QStringLiteral("<!DOCTYPE a [<!ATTLIST a b CDATA \"def\">]>\n<a/>");
    QTest::newRow("references") << QStringLiteral("<a>&#65;&#xA0;&apos;</a>");
    QTest::newRow("single-quotes") << QStringLiteral("<a b='say \"hi\"'/>");
    QTest::newRow("mixed") << QStringLiteral("<p>a <b>c</b> d</p>");
    QTest::newRow("cdata") << QStringLiteral("<a><![CDATA[ <x> ]]></a>");
    QTest::newRow("attribute-order") << QStringLiteral("<a z=\"1\"  y=\"2\"/>");
    QTest::newRow("non-ascii") << QStringLiteral("<a>M\u00FC \u20AC \U0001F600</a>");
}

void BodyFormatterTest::prettyXmlKeepsTheDocument()
{
    QFETCH(QString, text);

    QCOMPARE(prettyXml(text), text);
}

void BodyFormatterTest::prettyXmlIndentsStructure()
{
    QCOMPARE(prettyXml(QStringLiteral("<?xml version=\"1.0\"?><a><!-- n --><b><c/></b><?pi x?></a>")),
             QStringLiteral("<?xml version=\"1.0\"?>\n<a>\n  <!-- n -->\n  <b>\n    <c/>\n  </b>\n"
                            "  <?pi x?>\n</a>"));
}

void BodyFormatterTest::prettyJsonGivesUpOnHugeOutput()
{
    const QString nesting(1000, u'[');
    const QString closing(1000, u']');
    QString elements = QStringLiteral("1");
    elements += QStringLiteral(",1").repeated(200000);
    const QString text = nesting + elements + closing;

    QCOMPARE(prettyJson(text), text);
}

QTEST_GUILESS_MAIN(BodyFormatterTest)

#include "tst_bodyformatter.moc"
