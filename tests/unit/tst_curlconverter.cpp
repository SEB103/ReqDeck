// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "core/http/curlconverter.h"

/*!
 * \internal
 * \brief Returns a multi-line command as copied from a browser's developer tools.
 *
 * Uses CRLF line endings, backslash continuations, a cookie option, ANSI-C
 * quoting with an escaped newline, and the --compressed flag.
 */
static QString browserCommand()
{
    return QStringLiteral("curl 'https://api.example.test/v1/items?page=2' \\") + u"\r\n"
           + QStringLiteral("  -H 'accept: application/json' \\") + u"\r\n"
           + QStringLiteral("  -b 'sid=1' \\") + u"\r\n"
           + QStringLiteral("  -H 'content-type: application/json' \\") + u"\r\n"
           + QStringLiteral("  --data-raw $'{\"x\":\"a\\nb\"}' \\") + u"\r\n"
           + QStringLiteral("  --compressed");
}

/*!
 * \internal
 * \brief Returns a command using bash ANSI-C escapes as browsers emit them.
 *
 * Covers \uHHHH (Chrome writes '!' as !), \t, \xHH bytes forming UTF-8,
 * octal \nnn, and \UHHHHHHHH outside the BMP.
 */
static QString ansiCCommand()
{
    return QStringLiteral("curl -d $'password=abc\\u0021&t=a\\tb' "
                          "-H $'X-A: caf\\xc3\\xa9 \\101 \\U0001F600' https://h");
}

/*!
 * \internal
 * \brief Returns every valid command fixture used by the round-trip test.
 */
static QStringList validFixtures()
{
    return {
        QStringLiteral("curl https://h/x"),
        QStringLiteral("curl -X PUT https://h/x"),
        QStringLiteral("curl -d 'a=1' https://h"),
        QStringLiteral("curl -I https://h"),
        QStringLiteral("curl -XPATCH --url=https://h/x"),
        QStringLiteral("curl -H 'Accept: application/json' -H \"X-Tag: a\" -H 'X-Tag: b' -u user:p:w https://h"),
        QStringLiteral("curl https://h -H 'Content-Type: application/json' --data-raw '{\"a\": \"it'\\''s\"}'"),
        browserCommand(),
        QStringLiteral("curl -d '{bad' https://h"),
        QStringLiteral("curl --connect-timeout 5 -m 10 https://h"),
        QStringLiteral("curl --data 'q=a+b%26c' https://h"),
        QStringLiteral("curl -X GET -d 'x=1' https://h"),
        QStringLiteral("curl -sSL -A 'agent/1' -e https://ref.test https://h"),
        ansiCCommand(),
        QStringLiteral("curl --data-urlencode 'q=a b&c' -d 'n=1' https://h"),
        QStringLiteral("curl --json '{\"a\":1}' https://h"),
        QStringLiteral("curl -G -d 'q=1' -d 'r=2' 'https://h/x?a=0'"),
        QStringLiteral("curl --oauth2-bearer tok https://h"),
    };
}

/*! Verifies cURL command import and export. */
class CurlConverterTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that the cURL command follows redirects and times out like ReqDeck. */
    void toCurlFollowsAndTimesOut();

    /*! Verifies method, URL, and body mode of simple commands. */
    void parsesBasicForms_data();
    /*! Runs one simple-command case. */
    void parsesBasicForms();

    /*! Verifies headers in order and Basic credentials split at the first colon. */
    void parsesHeadersAndAuth();

    /*! Verifies a JSON body with an escaped single quote. */
    void parsesJsonBody();

    /*! Verifies commands copied from browser developer tools. */
    void parsesBrowserCopiedCommands();

    /*! Verifies that -A, -b, -e and their long forms become headers. */
    void mapsHeaderShorthands();

    /*! Verifies the full set of bash ANSI-C escapes in $'...' words. */
    void parsesAnsiCEscapes();

    /*! Verifies --data-urlencode encoding rules. */
    void parsesDataUrlencode();

    /*! Verifies that --json sets the body and both JSON headers. */
    void parsesJsonOption();

    /*! Verifies that -G moves data into the query string of a GET. */
    void getOptionMovesDataToQuery();

    /*! Verifies that --oauth2-bearer becomes an Authorization header. */
    void mapsOauth2BearerToHeader();

    /*! Verifies that --compressed drops a manual Accept-Encoding header. */
    void compressedDropsAcceptEncoding();

    /*! Verifies that non-form data keeps curl's default Content-Type. */
    void rawTextKeepsCurlDefaultContentType();

    /*! Verifies that form data is decoded into fields. */
    void decodesFormFields();

    /*! Verifies the errors reported for invalid commands. */
    void rejectsInvalidInput_data();
    /*! Runs one invalid-command case. */
    void rejectsInvalidInput();

    /*! Verifies that values of ignored options are not taken as the URL. */
    void skipsIgnoredOptionValues();

    /*! Verifies the exact layout and quoting of an exported command. */
    void emitsQuotedMultilineCommand();

    /*! Verifies exported auth, query parameters, and the omitted -X for GET. */
    void emitsAuthAndParams();

    /*! Verifies that parse, export, and parse again yields the same request. */
    void roundTripIsStable_data();
    /*! Runs one round-trip case. */
    void roundTripIsStable();
};

void CurlConverterTest::parsesBasicForms_data()
{
    QTest::addColumn<QString>("command");
    QTest::addColumn<HttpMethod>("method");
    QTest::addColumn<QString>("url");
    QTest::addColumn<BodyMode>("bodyMode");

    QTest::newRow("get") << QStringLiteral("curl https://h/x") << HttpMethod::Get
                         << QStringLiteral("https://h/x") << BodyMode::None;
    QTest::newRow("put") << QStringLiteral("curl -X PUT https://h/x") << HttpMethod::Put
                         << QStringLiteral("https://h/x") << BodyMode::None;
    QTest::newRow("data") << QStringLiteral("curl -d 'a=1' https://h") << HttpMethod::Post
                          << QStringLiteral("https://h") << BodyMode::FormUrlEncoded;
    QTest::newRow("head") << QStringLiteral("curl -I https://h") << HttpMethod::Head
                          << QStringLiteral("https://h") << BodyMode::None;
    QTest::newRow("attached") << QStringLiteral("curl -XPATCH --url=https://h/x") << HttpMethod::Patch
                              << QStringLiteral("https://h/x") << BodyMode::None;
}

void CurlConverterTest::parsesBasicForms()
{
    QFETCH(QString, command);
    QFETCH(HttpMethod, method);
    QFETCH(QString, url);
    QFETCH(BodyMode, bodyMode);

    const CurlConverter::ParseResult result = CurlConverter::parse(command);
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.method, method);
    QCOMPARE(result.request.url, url);
    QCOMPARE(result.request.body.mode, bodyMode);
}

void CurlConverterTest::parsesHeadersAndAuth()
{
    const CurlConverter::ParseResult result = CurlConverter::parse(
        QStringLiteral("curl -H 'Accept: application/json' -H \"X-Tag: a\" -H 'X-Tag: b' -u user:p:w https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    const QList<KeyValue> expected {
        {QStringLiteral("Accept"), QStringLiteral("application/json"), true},
        {QStringLiteral("X-Tag"), QStringLiteral("a"), true},
        {QStringLiteral("X-Tag"), QStringLiteral("b"), true},
    };
    QVERIFY(result.request.headers == expected);
    QCOMPARE(result.request.auth.type, AuthType::Basic);
    QCOMPARE(result.request.auth.username, QStringLiteral("user"));
    QCOMPARE(result.request.auth.password, QStringLiteral("p:w"));
}

void CurlConverterTest::parsesJsonBody()
{
    const CurlConverter::ParseResult result = CurlConverter::parse(
        QStringLiteral("curl https://h -H 'Content-Type: application/json' --data-raw '{\"a\": \"it'\\''s\"}'"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.method, HttpMethod::Post);
    QCOMPARE(result.request.body.mode, BodyMode::Raw);
    QCOMPARE(result.request.body.rawType, RawBodyType::Json);
    QCOMPARE(result.request.body.raw, QStringLiteral("{\"a\": \"it's\"}"));
}

void CurlConverterTest::parsesBrowserCopiedCommands()
{
    const CurlConverter::ParseResult result = CurlConverter::parse(browserCommand());
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.url, QStringLiteral("https://api.example.test/v1/items?page=2"));
    QCOMPARE(result.request.method, HttpMethod::Post);
    const QList<KeyValue> expected {
        {QStringLiteral("accept"), QStringLiteral("application/json"), true},
        {QStringLiteral("Cookie"), QStringLiteral("sid=1"), true},
        {QStringLiteral("content-type"), QStringLiteral("application/json"), true},
    };
    QVERIFY(result.request.headers == expected);
    QCOMPARE(result.request.body.mode, BodyMode::Raw);
    QCOMPARE(result.request.body.rawType, RawBodyType::Json);
    QCOMPARE(result.request.body.raw, QStringLiteral("{\"x\":\"a\nb\"}"));
    QVERIFY(result.ignoredOptions.contains(QStringLiteral("--compressed")));
}

void CurlConverterTest::mapsHeaderShorthands()
{
    const CurlConverter::ParseResult result = CurlConverter::parse(QStringLiteral(
        "curl --user-agent a/1 -A b/2 --cookie c=1 -bd=2 --referer r1 -e r2 https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    const QList<KeyValue> expected {
        {QStringLiteral("User-Agent"), QStringLiteral("a/1"), true},
        {QStringLiteral("User-Agent"), QStringLiteral("b/2"), true},
        {QStringLiteral("Cookie"), QStringLiteral("c=1"), true},
        {QStringLiteral("Cookie"), QStringLiteral("d=2"), true},
        {QStringLiteral("Referer"), QStringLiteral("r1"), true},
        {QStringLiteral("Referer"), QStringLiteral("r2"), true},
    };
    QVERIFY(result.request.headers == expected);
}

void CurlConverterTest::parsesAnsiCEscapes()
{
    const CurlConverter::ParseResult result = CurlConverter::parse(ansiCCommand());
    QVERIFY2(result.ok, qPrintable(result.errorString));
    const QList<KeyValue> form {
        {QStringLiteral("password"), QStringLiteral("abc!"), true},
        {QStringLiteral("t"), QStringLiteral("a\tb"), true},
    };
    QVERIFY(result.request.body.form == form);
    const QList<KeyValue> headers {
        {QStringLiteral("X-A"), QStringLiteral("café A \U0001F600"), true},
    };
    QVERIFY(result.request.headers == headers);
}

void CurlConverterTest::parsesDataUrlencode()
{
    const CurlConverter::ParseResult result =
        CurlConverter::parse(QStringLiteral("curl --data-urlencode 'q=a b&c' -d 'n=1' https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.method, HttpMethod::Post);
    const QList<KeyValue> form {
        {QStringLiteral("q"), QStringLiteral("a b&c"), true},
        {QStringLiteral("n"), QStringLiteral("1"), true},
    };
    QVERIFY(result.request.body.form == form);
}

void CurlConverterTest::parsesJsonOption()
{
    const CurlConverter::ParseResult result =
        CurlConverter::parse(QStringLiteral("curl --json '{\"a\":1}' https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.method, HttpMethod::Post);
    QCOMPARE(result.request.body.mode, BodyMode::Raw);
    QCOMPARE(result.request.body.rawType, RawBodyType::Json);
    QCOMPARE(result.request.body.raw, QStringLiteral("{\"a\":1}"));
    const QList<KeyValue> headers {
        {QStringLiteral("Content-Type"), QStringLiteral("application/json"), true},
        {QStringLiteral("Accept"), QStringLiteral("application/json"), true},
    };
    QVERIFY(result.request.headers == headers);
}

void CurlConverterTest::getOptionMovesDataToQuery()
{
    const CurlConverter::ParseResult result =
        CurlConverter::parse(QStringLiteral("curl -G -d 'q=1' -d 'r=2' 'https://h/x?a=0'"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.method, HttpMethod::Get);
    QCOMPARE(result.request.url, QStringLiteral("https://h/x?a=0&q=1&r=2"));
    QCOMPARE(result.request.body.mode, BodyMode::None);
}

void CurlConverterTest::mapsOauth2BearerToHeader()
{
    const CurlConverter::ParseResult result =
        CurlConverter::parse(QStringLiteral("curl --oauth2-bearer tok https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.url, QStringLiteral("https://h"));
    const QList<KeyValue> headers {
        {QStringLiteral("Authorization"), QStringLiteral("Bearer tok"), true},
    };
    QVERIFY(result.request.headers == headers);
}

void CurlConverterTest::compressedDropsAcceptEncoding()
{
    CurlConverter::ParseResult result = CurlConverter::parse(
        QStringLiteral("curl --compressed -H 'Accept-Encoding: gzip, br' -H 'X-A: 1' https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    const QList<KeyValue> headers {
        {QStringLiteral("X-A"), QStringLiteral("1"), true},
    };
    QVERIFY(result.request.headers == headers);

    result = CurlConverter::parse(QStringLiteral("curl -H 'Accept-Encoding: identity' https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.headers.size(), 1);
}

void CurlConverterTest::rawTextKeepsCurlDefaultContentType()
{
    const CurlConverter::ParseResult result = CurlConverter::parse(QStringLiteral("curl -d '{bad' https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.body.mode, BodyMode::Raw);
    QCOMPARE(result.request.body.rawType, RawBodyType::Text);
    QCOMPARE(result.request.body.raw, QStringLiteral("{bad"));
    const QList<KeyValue> expected {
        {QStringLiteral("Content-Type"), QStringLiteral("application/x-www-form-urlencoded"), true},
    };
    QVERIFY(result.request.headers == expected);
}

void CurlConverterTest::decodesFormFields()
{
    const CurlConverter::ParseResult result =
        CurlConverter::parse(QStringLiteral("curl --data 'q=a+b%26c' -d 'n=' https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.body.mode, BodyMode::FormUrlEncoded);
    const QList<KeyValue> expected {
        {QStringLiteral("q"), QStringLiteral("a b&c"), true},
        {QStringLiteral("n"), QString(), true},
    };
    QVERIFY(result.request.body.form == expected);
    QVERIFY(result.request.headers.isEmpty());
}

void CurlConverterTest::rejectsInvalidInput_data()
{
    QTest::addColumn<QString>("command");
    QTest::addColumn<QString>("error");

    QTest::newRow("empty") << QString() << QStringLiteral("Not a cURL command.");
    QTest::newRow("wget") << QStringLiteral("wget http://h") << QStringLiteral("Not a cURL command.");
    QTest::newRow("quote") << QStringLiteral("curl 'http://h") << QStringLiteral("Unterminated quote.");
    QTest::newRow("method") << QStringLiteral("curl -X FETCH http://h")
                            << QStringLiteral("Unsupported method: FETCH");
    QTest::newRow("file") << QStringLiteral("curl -d @file.json http://h")
                          << QStringLiteral("File uploads (@file) are not supported.");
    QTest::newRow("no-url") << QStringLiteral("curl -H 'A: b'") << QStringLiteral("No URL found.");
    QTest::newRow("urlencode-file") << QStringLiteral("curl --data-urlencode 'q@file.txt' http://h")
                                    << QStringLiteral("File uploads (@file) are not supported.");
    QTest::newRow("multipart") << QStringLiteral("curl -F 'a=1' http://h")
                               << QStringLiteral("Multipart form data (-F) is not supported.");
    QTest::newRow("upload") << QStringLiteral("curl -T f.txt http://h")
                            << QStringLiteral("File uploads (-T) are not supported.");
    QTest::newRow("cmd") << QStringLiteral("curl ^\"https://h^\" -H ^\"A: b^\"")
                         << QStringLiteral("Windows cmd syntax is not supported; copy the command as cURL (bash).");
}

void CurlConverterTest::rejectsInvalidInput()
{
    QFETCH(QString, command);
    QFETCH(QString, error);

    const CurlConverter::ParseResult result = CurlConverter::parse(command);
    QVERIFY(!result.ok);
    QCOMPARE(result.errorString, error);
}

void CurlConverterTest::skipsIgnoredOptionValues()
{
    const CurlConverter::ParseResult result =
        CurlConverter::parse(QStringLiteral(
            "curl --connect-timeout 5 -m 10 --cacert ca.pem -c jar.txt -E cert.pem --max-redirs 3 https://h"));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.request.url, QStringLiteral("https://h"));
    QVERIFY(result.ignoredOptions.contains(QStringLiteral("--connect-timeout")));
    QVERIFY(result.ignoredOptions.contains(QStringLiteral("-m")));
    QVERIFY(result.ignoredOptions.contains(QStringLiteral("--cacert")));
    QVERIFY(result.ignoredOptions.contains(QStringLiteral("-c")));
}

void CurlConverterTest::emitsQuotedMultilineCommand()
{
    RequestSpec spec;
    spec.method = HttpMethod::Post;
    spec.url = QStringLiteral("https://h/x");
    spec.headers.append({QStringLiteral("X-A"), QStringLiteral("it's"), true});
    spec.body.mode = BodyMode::Raw;
    spec.body.rawType = RawBodyType::Json;
    spec.body.raw = QStringLiteral("{}");

    QCOMPARE(CurlConverter::toCurl(spec),
             QStringLiteral("curl \\\n  -X 'POST' \\\n  'https://h/x' \\\n  -H 'X-A: it'\\''s' \\\n"
                            "  -H 'Content-Type: application/json' \\\n  --data-raw '{}'"));
}

void CurlConverterTest::emitsAuthAndParams()
{
    RequestSpec spec;
    spec.url = QStringLiteral("https://h/x");
    spec.queryParams.append({QStringLiteral("q"), QStringLiteral("a b"), true});
    spec.auth.type = AuthType::Bearer;
    spec.auth.token = QStringLiteral("t");

    const QString command = CurlConverter::toCurl(spec);
    QVERIFY2(command.contains(QStringLiteral("'https://h/x?q=a%20b'")), qPrintable(command));
    QVERIFY2(command.contains(QStringLiteral("-H 'Authorization: Bearer t'")), qPrintable(command));
    QVERIFY2(!command.contains(QStringLiteral("-X")), qPrintable(command));
}

void CurlConverterTest::roundTripIsStable_data()
{
    QTest::addColumn<QString>("command");

    const QStringList fixtures = validFixtures();
    for (qsizetype i = 0; i < fixtures.size(); ++i)
        QTest::addRow("fixture-%lld", static_cast<long long>(i)) << fixtures.at(i);
}

void CurlConverterTest::roundTripIsStable()
{
    QFETCH(QString, command);

    const CurlConverter::ParseResult first = CurlConverter::parse(command);
    QVERIFY2(first.ok, qPrintable(first.errorString));
    const QString exported = CurlConverter::toCurl(first.request);
    const CurlConverter::ParseResult second = CurlConverter::parse(exported);
    QVERIFY2(second.ok, qPrintable(second.errorString));
    QVERIFY2(second.request == first.request, qPrintable(exported));
}

void CurlConverterTest::toCurlFollowsAndTimesOut()
{
    RequestSpec spec;
    spec.url = QStringLiteral("https://h/x");

    QVERIFY(!CurlConverter::toCurl(spec).contains(QStringLiteral(" -L")));
    QVERIFY(!CurlConverter::toCurl(spec).contains(QStringLiteral(" -m ")));

    const QString defaults = CurlConverter::toCurl(spec, true, 30000);
    QVERIFY2(defaults.contains(QStringLiteral("  -L \\\n")), qPrintable(defaults));
    QVERIFY2(defaults.contains(QStringLiteral("  -m 30 \\\n")), qPrintable(defaults));

    spec.timeoutMs = 1500;
    const QString own = CurlConverter::toCurl(spec, false, 30000);
    QVERIFY2(own.contains(QStringLiteral("  -m 1.5 \\\n")), qPrintable(own));
    QVERIFY2(!own.contains(QStringLiteral(" -L")), qPrintable(own));
    QVERIFY(CurlConverter::parse(own).ok);
}

QTEST_GUILESS_MAIN(CurlConverterTest)

#include "tst_curlconverter.moc"
