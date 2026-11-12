// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include "core/http/responsedata.h"
#include "framework/bodyformatter.h"
#include "qmlapi/keyvaluemodel.h"
#include "qmlapi/responseview.h"

/*!
 * \internal
 * \brief Returns a 200 response with \a body of \a contentType.
 */
static ResponseData makeResponse(const QByteArray &body, const QString &contentType)
{
    ResponseData response;
    response.statusCode = 200;
    response.reasonPhrase = QStringLiteral("OK");
    response.body = body;
    response.sizeBytes = body.size();
    response.contentType = contentType;
    response.elapsedMs = 42;
    response.headers = {{"content-type", contentType.toUtf8()}, {"x-a", "1"}};
    return response;
}

/*! Verifies how a response is prepared for the response pane. */
class ResponseViewTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies the state before anything was sent. */
    void startsEmpty();

    /*! Verifies the state while a request runs. */
    void busyNamesRequest();

    /*! Verifies status and timing of a response. */
    void exposesStatusAndTiming();

    /*! Verifies the status class. */
    void statusClassFollowsCode_data();
    /*! Runs one status class case. */
    void statusClassFollowsCode();

    /*! Verifies readable sizes. */
    void sizeTextUsesReadableUnits_data();
    /*! Runs one size case. */
    void sizeTextUsesReadableUnits();

    /*! Verifies the switch between Pretty and Raw. */
    void prettyAndRawSwitchWithoutReformatting();

    /*! Verifies that the pretty setting survives the next response. */
    void prettyChoiceSurvivesNextResponse();

    /*! Verifies a body labeled JSON that is not. */
    void invalidJsonStaysRaw();

    /*! Verifies that plain text offers no Pretty. */
    void plainTextCannotBePretty();

    /*! Verifies that a body that cannot be formatted offers no Pretty. */
    void unformattableBodyCannotBePretty_data();
    /*! Runs one unformattable case. */
    void unformattableBodyCannotBePretty();

    /*! Verifies when the view must break lines inside words. */
    void wrapAnywhereFollowsLongestUnbrokenRun_data();
    /*! Runs one wrap case. */
    void wrapAnywhereFollowsLongestUnbrokenRun();

    /*! Verifies the display limit. */
    void longTextIsCutAtDisplayLimit();

    /*! Verifies that binary data is not shown. */
    void binaryBodyShowsNoticeOnly();

    /*! Verifies the error of a failed transfer. */
    void transportErrorShowsMessageAndTlsDetails();

    /*! Verifies a response that broke off after the status. */
    void truncatedBodyShowsBodyAndError();

    /*! Verifies a request that could not be built. */
    void buildErrorShowsMessageWithoutStatus();

    /*! Verifies the warning about unknown variables. */
    void unresolvedVariablesBecomeWarning();

    /*! Verifies that stored cookies left out for a typed Cookie header give a warning. */
    void skippedCookiesBecomeWarning();

    /*! Verifies the JSONPath filter: matches, no match, errors, and that it stays for the next response. */
    void filterShowsMatches();

    /*! Verifies that find ignores case and reports positions in the shown text. */
    void findIsCaseInsensitive();
    /*! Verifies that find stops at its limit. */
    void findStopsAtLimit();

    /*! Verifies that the cut at the display limit does not split a character in two. */
    void displayLimitKeepsSurrogatePairs();

    /*! Verifies that saving writes the body as received, not the formatted text. */
    void saveBodyWritesBytes();
    /*! Verifies that saving a body on disk copies the whole file. */
    void saveSpilledBodyCopiesFile();
    /*! Verifies that a failed save says where and why, and keeps no partial file. */
    void saveFailsWithMessage();
    /*! Verifies that each server is named with its own certificate outcome. */
    void certificateOutcomesNameTheirServer();
    /*! Verifies that saving needs body data, not only a size. */
    void saveNeedsHeldData();
    /*! Verifies that only local files are written. */
    void saveRejectsNonLocalUrl();
    /*! Provides URLs and kinds of body with the file name they suggest. */
    void suggestedFileNameFollowsUrlAndKind_data();
    /*! Verifies the suggested file name. */
    void suggestedFileNameFollowsUrlAndKind();

    /*! Verifies that a body on disk shows its start, its whole size, and no formatting. */
    void bodyOnDiskShowsStart();

    /*! Verifies that only a failed certificate check offers to continue, for its host. */
    void certificateCanBeTrustedAfterTlsFailure();

    /*! Verifies that ignored certificate errors become a warning naming the host. */
    void ignoredCertificateBecomesWarning();

    /*! Verifies the header table. */
    void headersModelListsResponseHeaders();

    /*! Verifies clear(). */
    void clearResets();
};

void ResponseViewTest::startsEmpty()
{
    const ResponseView view;
    QVERIFY(!view.property("hasResponse").toBool());
    QVERIFY(!view.property("busy").toBool());
    QVERIFY(view.property("pretty").toBool());
    QCOMPARE(view.property("statusClass").toInt(), 0);
    QVERIFY(view.property("bodyText").toString().isEmpty());
}

void ResponseViewTest::busyNamesRequest()
{
    ResponseView view;
    QSignalSpy spy(&view, &ResponseView::responseChanged);
    view.setBusy(QStringLiteral("List users"), QStringLiteral("id-1"));
    QCOMPARE(view.requestId(), QStringLiteral("id-1"));

    QVERIFY(view.property("busy").toBool());
    QVERIFY(!view.property("hasResponse").toBool());
    QCOMPARE(view.property("requestName").toString(), QStringLiteral("List users"));
    QCOMPARE(spy.count(), 1);
}

void ResponseViewTest::exposesStatusAndTiming()
{
    ResponseView view;
    view.setBusy(QStringLiteral("List"));
    view.setResponse(makeResponse("{}", QStringLiteral("application/json")), {});

    QVERIFY(view.property("hasResponse").toBool());
    QVERIFY(!view.property("busy").toBool());
    QCOMPARE(view.property("requestName").toString(), QStringLiteral("List"));
    QCOMPARE(view.property("statusCode").toInt(), 200);
    QCOMPARE(view.property("reasonPhrase").toString(), QStringLiteral("OK"));
    QCOMPARE(view.property("elapsedMs").toLongLong(), 42);
    QCOMPARE(view.property("sizeBytes").toLongLong(), 2);
    QCOMPARE(view.property("contentType").toString(), QStringLiteral("application/json"));
    QCOMPARE(view.property("bodyKind").toInt(), int(BodyKind::Json));
    QVERIFY(view.property("errorText").toString().isEmpty());
}

void ResponseViewTest::statusClassFollowsCode_data()
{
    QTest::addColumn<int>("code");
    QTest::addColumn<int>("statusClass");

    QTest::newRow("101") << 101 << 1;
    QTest::newRow("204") << 204 << 2;
    QTest::newRow("302") << 302 << 3;
    QTest::newRow("404") << 404 << 4;
    QTest::newRow("503") << 503 << 5;
    QTest::newRow("none") << 0 << 0;
}

void ResponseViewTest::statusClassFollowsCode()
{
    QFETCH(int, code);
    QFETCH(int, statusClass);

    ResponseData response;
    response.statusCode = code;
    ResponseView view;
    view.setResponse(response, {});
    QCOMPARE(view.property("statusClass").toInt(), statusClass);
}

void ResponseViewTest::sizeTextUsesReadableUnits_data()
{
    QTest::addColumn<qint64>("bytes");
    QTest::addColumn<QString>("text");

    QTest::newRow("bytes") << qint64(512) << QStringLiteral("512 B");
    QTest::newRow("kilo") << qint64(1536) << QStringLiteral("1.5 KB");
    QTest::newRow("mega") << qint64(2097152) << QStringLiteral("2.0 MB");
}

void ResponseViewTest::sizeTextUsesReadableUnits()
{
    QFETCH(qint64, bytes);
    QFETCH(QString, text);

    ResponseData response;
    response.sizeBytes = bytes;
    ResponseView view;
    view.setResponse(response, {});
    QCOMPARE(view.property("sizeText").toString(), text);
}

void ResponseViewTest::prettyAndRawSwitchWithoutReformatting()
{
    ResponseView view;
    view.setResponse(makeResponse("{\"b\":1,\"a\":2}", QStringLiteral("application/json")), {});

    QVERIFY(view.property("canPretty").toBool());
    QCOMPARE(view.property("bodyText").toString(), QStringLiteral("{\n  \"b\": 1,\n  \"a\": 2\n}"));

    QSignalSpy spy(&view, &ResponseView::responseChanged);
    view.setProperty("pretty", false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(view.property("bodyText").toString(), QStringLiteral("{\"b\":1,\"a\":2}"));
}

void ResponseViewTest::prettyChoiceSurvivesNextResponse()
{
    ResponseView view;
    view.setProperty("pretty", false);
    view.setResponse(makeResponse("{\"a\":1}", QStringLiteral("application/json")), {});
    QCOMPARE(view.property("bodyText").toString(), QStringLiteral("{\"a\":1}"));
}

void ResponseViewTest::invalidJsonStaysRaw()
{
    ResponseView view;
    view.setResponse(makeResponse("{oops", QStringLiteral("application/json")), {});
    QCOMPARE(view.property("bodyText").toString(), QStringLiteral("{oops"));
    QVERIFY(view.property("errorText").toString().isEmpty());
}

void ResponseViewTest::plainTextCannotBePretty()
{
    ResponseView view;
    view.setResponse(makeResponse("hello", QStringLiteral("text/plain")), {});
    QVERIFY(!view.property("canPretty").toBool());
    QCOMPARE(view.property("bodyText").toString(), QStringLiteral("hello"));
}

void ResponseViewTest::unformattableBodyCannotBePretty_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("contentType");

    QTest::newRow("broken-json") << QByteArray("{oops") << QStringLiteral("application/json");
    QTest::newRow("ndjson") << QByteArray("{}\n{}") << QStringLiteral("application/x-ndjson");
    QTest::newRow("empty") << QByteArray() << QStringLiteral("application/json");
    QTest::newRow("broken-xml") << QByteArray("<a>") << QStringLiteral("application/xml");
}

void ResponseViewTest::unformattableBodyCannotBePretty()
{
    QFETCH(QByteArray, body);
    QFETCH(QString, contentType);

    ResponseView view;
    view.setResponse(makeResponse(body, contentType), {});
    QVERIFY(!view.property("canPretty").toBool());
}

void ResponseViewTest::wrapAnywhereFollowsLongestUnbrokenRun_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<bool>("wrapAnywhere");

    QTest::newRow("prose") << QByteArray("some words").repeated(1000) << false;
    QTest::newRow("spaced") << QByteArray("word ").repeated(100000) << false;
    QTest::newRow("short-run") << QByteArray(1024, 'x') << false;
    QTest::newRow("long-run") << QByteArray(1025, 'x') << true;
    QTest::newRow("late-run") << QByteArray("a b\n") + QByteArray(2000, 'x') << true;
}

void ResponseViewTest::wrapAnywhereFollowsLongestUnbrokenRun()
{
    QFETCH(QByteArray, body);
    QFETCH(bool, wrapAnywhere);

    ResponseView view;
    view.setResponse(makeResponse(body, QStringLiteral("text/plain")), {});
    QCOMPARE(view.property("wrapAnywhere").toBool(), wrapAnywhere);
}

void ResponseViewTest::longTextIsCutAtDisplayLimit()
{
    ResponseView view;
    QByteArray body(kMaxDisplayChars + 10, 'x');
    view.setResponse(makeResponse(body, QStringLiteral("text/plain")), {});
    QCOMPARE(view.property("bodyText").toString().size(), kMaxDisplayChars);
    QVERIFY(view.property("truncated").toBool());

    view.setResponse(makeResponse("short", QStringLiteral("text/plain")), {});
    QVERIFY(!view.property("truncated").toBool());

    // Too long to format: stays raw instead of building a larger string.
    body = "[" + QByteArray(kMaxDisplayChars, '1') + "]";
    view.setResponse(makeResponse(body, QStringLiteral("application/json")), {});
    QVERIFY(!view.property("canPretty").toBool());
    QVERIFY(view.property("truncated").toBool());
}

void ResponseViewTest::binaryBodyShowsNoticeOnly()
{
    ResponseView view;
    view.setResponse(makeResponse(QByteArray("ab\0cd", 5), QStringLiteral("image/png")), {});
    QCOMPARE(view.property("bodyKind").toInt(), int(BodyKind::Binary));
    QCOMPARE(view.property("bodyText").toString(), QStringLiteral("Binary body, 5 B. Not shown."));
    QVERIFY(!view.property("canPretty").toBool());
}

/*!
 * \internal
 * \brief Returns the certificate outcome of \a host at \a port with \a messages.
 */
static TlsCheck tlsCheck(const QString &host, int port, const QStringList &messages, bool ignored)
{
    TlsCheck check;
    check.host = host;
    check.port = port;
    check.messages = messages;
    check.ignored = ignored;
    return check;
}

void ResponseViewTest::transportErrorShowsMessageAndTlsDetails()
{
    ResponseData response;
    response.error = ResponseError::Tls;
    response.errorString = QStringLiteral("Handshake failed");
    response.tlsChecks = {tlsCheck(QStringLiteral("dev.local"), 443,
                                   {QStringLiteral("Expired"), QStringLiteral("Wrong host")}, false)};

    ResponseView view;
    view.setResponse(response, {});
    QVERIFY(view.property("hasResponse").toBool());
    QCOMPARE(view.property("errorText").toString(),
             QStringLiteral("Handshake failed\nExpired\nWrong host"));
}

void ResponseViewTest::truncatedBodyShowsBodyAndError()
{
    ResponseData response = makeResponse("partial", QStringLiteral("text/plain"));
    response.error = ResponseError::Network;
    response.errorString = QStringLiteral("Connection closed");

    ResponseView view;
    view.setResponse(response, {});
    QCOMPARE(view.property("statusCode").toInt(), 200);
    QCOMPARE(view.property("bodyText").toString(), QStringLiteral("partial"));
    QCOMPARE(view.property("errorText").toString(), QStringLiteral("Connection closed"));
}

void ResponseViewTest::buildErrorShowsMessageWithoutStatus()
{
    ResponseView view;
    view.setBuildError(QStringLiteral("URL is empty."), {});
    QVERIFY(view.property("hasResponse").toBool());
    QVERIFY(!view.property("busy").toBool());
    QCOMPARE(view.property("statusCode").toInt(), 0);
    QCOMPARE(view.property("errorText").toString(), QStringLiteral("URL is empty."));
}

void ResponseViewTest::filterShowsMatches()
{
    ResponseView view;
    view.setResponse(makeResponse(R"({"items":[{"id":1},{"id":2}]})", QStringLiteral("application/json")), {});
    QVERIFY(view.property("canFilter").toBool());

    view.setProperty("filterText", QStringLiteral("$.items[*].id"));
    QCOMPARE(QJsonDocument::fromJson(view.bodyText().toUtf8()).array(), QJsonArray({1, 2}));
    QVERIFY(view.property("filterError").toString().isEmpty());

    view.setProperty("filterText", QStringLiteral("$.nothing"));
    QCOMPARE(view.bodyText(), tr("No match."));
    view.setProperty("filterText", QStringLiteral("items"));
    QVERIFY(!view.property("filterError").toString().isEmpty());

    view.setProperty("filterText", QStringLiteral("$.items[0].id"));
    view.setResponse(makeResponse(R"({"items":[{"id":7}]})", QStringLiteral("application/json")), {});
    QCOMPARE(QJsonDocument::fromJson(view.bodyText().toUtf8()).array(), QJsonArray({7}));

    view.setResponse(makeResponse("plain", QStringLiteral("text/plain")), {});
    QVERIFY(!view.property("canFilter").toBool());
    QCOMPARE(view.bodyText(), QStringLiteral("plain"));
}

void ResponseViewTest::skippedCookiesBecomeWarning()
{
    ResponseView view;
    ResponseData response = makeResponse("{}", QStringLiteral("application/json"));
    response.storedCookiesSkipped = true;
    view.setResponse(response, {});
    QVERIFY2(view.property("warningText").toString().contains(QStringLiteral("Stored cookies were not sent")),
             qPrintable(view.property("warningText").toString()));
}

void ResponseViewTest::unresolvedVariablesBecomeWarning()
{
    ResponseView view;
    view.setResponse(makeResponse("{}", QStringLiteral("application/json")),
                     {QStringLiteral("a"), QStringLiteral("b")});
    QCOMPARE(view.property("warningText").toString(),
             QStringLiteral("Unresolved variables: {{a}}, {{b}}"));

    view.setResponse(makeResponse("{}", QStringLiteral("application/json")), {});
    QVERIFY(view.property("warningText").toString().isEmpty());
}

void ResponseViewTest::findIsCaseInsensitive()
{
    ResponseView view;
    view.setResponse(makeResponse("Abc abc ABC", QStringLiteral("text/plain")), {});

    QCOMPARE(view.findInBody(QStringLiteral("abc")), QVariantList({0, 4, 8}));
    QCOMPARE(view.findInBody(QStringLiteral("BC A")), QVariantList({1, 5}));
    QVERIFY(view.findInBody(QString()).isEmpty());
    QVERIFY(view.findInBody(QStringLiteral("zzz")).isEmpty());

    // Positions refer to the text shown, here the formatted one.
    view.setResponse(makeResponse("{\"a\":1}", QStringLiteral("application/json")), {});
    const QString shown = view.property("bodyText").toString();
    const QVariantList matches = view.findInBody(QStringLiteral("\"a\""));
    QCOMPARE(matches.size(), 1);
    QCOMPARE(shown.mid(matches.first().toInt(), 3), QStringLiteral("\"a\""));

    // The text view keeps one separator for CRLF, so positions count it once.
    view.setResponse(makeResponse("a\r\nb\r\nb", QStringLiteral("text/plain")), {});
    QCOMPARE(view.findInBody(QStringLiteral("b")), QVariantList({2, 4}));
}

void ResponseViewTest::displayLimitKeepsSurrogatePairs()
{
    const QString text = QString(kMaxDisplayChars - 1, u'x') + QStringLiteral("\U0001F600 tail");
    ResponseView view;
    view.setResponse(makeResponse(text.toUtf8(), QStringLiteral("text/plain")), {});

    const QString shown = view.property("bodyText").toString();
    QCOMPARE(shown.size(), kMaxDisplayChars - 1);
    QVERIFY(!shown.back().isHighSurrogate());
    QVERIFY(view.property("truncated").toBool());
}

void ResponseViewTest::findStopsAtLimit()
{
    ResponseView view;
    view.setResponse(makeResponse(QByteArray(ResponseView::kMaxFindMatches + 50, 'x'), QStringLiteral("text/plain")), {});

    QCOMPARE(view.findInBody(QStringLiteral("x")).size(), ResponseView::kMaxFindMatches);
}

void ResponseViewTest::saveBodyWritesBytes()
{
    ResponseView view;
    QCOMPARE(view.property("canSaveBody").toBool(), false);
    view.setResponse(makeResponse("{\"a\":1}", QStringLiteral("application/json")), {});
    QCOMPARE(view.property("canSaveBody").toBool(), true);
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("r.json"));
    QSignalSpy saved(&view, &ResponseView::bodySaved);

    QVERIFY(view.saveBody(QUrl::fromLocalFile(path)));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("{\"a\":1}"));
    QCOMPARE(saved.count(), 1);
    QCOMPARE(saved.first().first().toString(), QDir::toNativeSeparators(path));
}

void ResponseViewTest::saveSpilledBodyCopiesFile()
{
    ResponseData response = makeResponse("whole", QStringLiteral("text/plain"));
    response.bodyFile = std::make_shared<QTemporaryFile>();
    QVERIFY(response.bodyFile->open());
    const QByteArray whole = QByteArray(3 * 1024 * 1024, 'x') + "end";
    QCOMPARE(response.bodyFile->write(whole), whole.size());
    QVERIFY(response.bodyFile->flush());
    response.sizeBytes = whole.size();
    ResponseView view;
    view.setResponse(response, {});
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("big.txt"));

    QVERIFY(view.saveBody(QUrl::fromLocalFile(path)));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), whole);
}

void ResponseViewTest::saveFailsWithMessage()
{
    ResponseView view;
    view.setResponse(makeResponse("{}", QStringLiteral("application/json")), {});
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("missing/r.json"));
    QSignalSpy failed(&view, &ResponseView::bodySaveFailed);

    QVERIFY(!view.saveBody(QUrl::fromLocalFile(path)));

    QCOMPARE(failed.count(), 1);
    const QString message = failed.first().first().toString();
    QVERIFY2(message.contains(QDir::toNativeSeparators(path)), qPrintable(message));
    QVERIFY(!QFile::exists(path));
}

void ResponseViewTest::certificateOutcomesNameTheirServer()
{
    // A trusted first server redirected to a second one whose certificate failed.
    ResponseData response;
    response.error = ResponseError::Tls;
    response.errorString = QStringLiteral("Handshake failed");
    response.tlsChecks = {tlsCheck(QStringLiteral("dev.local"), 8443, {QStringLiteral("Self-signed")}, true),
                          tlsCheck(QStringLiteral("other.local"), 443, {QStringLiteral("Expired")}, false)};
    response.finalUrl = QUrl(QStringLiteral("https://other.local/items"));
    ResponseView view;

    view.setResponse(response, {});

    QCOMPARE(view.property("warningText").toString(),
             QStringLiteral("The certificate of dev.local:8443 was not checked: Self-signed"));
    QCOMPARE(view.property("errorText").toString(), QStringLiteral("Handshake failed\nother.local: Expired"));
    QCOMPARE(view.property("canTrustCertificate").toBool(), false);
}

void ResponseViewTest::saveNeedsHeldData()
{
    ResponseData response = makeResponse(QByteArray(), QStringLiteral("text/plain"));
    response.sizeBytes = 1000;
    ResponseView view;
    view.setResponse(response, {});

    QCOMPARE(view.property("canSaveBody").toBool(), false);
}

void ResponseViewTest::saveRejectsNonLocalUrl()
{
    ResponseView view;
    view.setResponse(makeResponse("{}", QStringLiteral("application/json")), {});
    QSignalSpy failed(&view, &ResponseView::bodySaveFailed);

    QVERIFY(!view.saveBody(QUrl(QStringLiteral("https://example.com/r.json"))));
    QVERIFY(!view.saveBody(QUrl(QStringLiteral("r.json"))));

    QCOMPARE(failed.count(), 2);
}

void ResponseViewTest::suggestedFileNameFollowsUrlAndKind_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("contentType");
    QTest::addColumn<QString>("name");

    QTest::newRow("json") << "https://api.example.com/v1/users" << QByteArray("{}")
                          << "application/json" << "users.json";
    QTest::newRow("root") << "https://api.example.com/" << QByteArray("{}") << "application/json"
                          << "response.json";
    QTest::newRow("xml") << "https://x/feed" << QByteArray("<a/>") << "application/xml" << "feed.xml";
    QTest::newRow("html") << "https://x/page" << QByteArray("<p>") << "text/html; charset=utf-8" << "page.html";
    QTest::newRow("text") << "https://x/notes" << QByteArray("hi") << "text/plain" << "notes.txt";
    QTest::newRow("has extension") << "https://x/files/report.pdf" << QByteArray("%PDF\0\1")
                                   << "application/pdf" << "report.pdf";
    QTest::newRow("binary") << "https://x/blob" << QByteArray("\0\1\2", 3) << "application/octet-stream"
                            << "blob.bin";
    QTest::newRow("unsafe") << "https://x/a%3Fb%3Ac" << QByteArray("hi") << "text/plain" << "a_b_c.txt";
    QTest::newRow("reserved") << "https://x/con" << QByteArray("hi") << "text/plain" << "con_.txt";
    QTest::newRow("reserved with extension") << "https://x/NUL.json" << QByteArray("{}") << "application/json"
                                             << "NUL_.json";
    QTest::newRow("reserved port") << "https://x/com1.txt" << QByteArray("hi") << "text/plain" << "com1_.txt";
    QTest::newRow("not reserved") << "https://x/console" << QByteArray("hi") << "text/plain" << "console.txt";
}

void ResponseViewTest::suggestedFileNameFollowsUrlAndKind()
{
    QFETCH(QString, url);
    QFETCH(QByteArray, body);
    QFETCH(QString, contentType);
    QFETCH(QString, name);

    ResponseData response = makeResponse(body, contentType);
    response.finalUrl = QUrl(url);
    ResponseView view;
    view.setResponse(response, {});

    QCOMPARE(view.suggestedFileName(), name);
}

void ResponseViewTest::bodyOnDiskShowsStart()
{
    ResponseData response = makeResponse("[1, 2", QStringLiteral("application/json"));
    response.sizeBytes = 200LL * 1024 * 1024;
    response.bodyFile = std::make_shared<QTemporaryFile>();
    QVERIFY(response.bodyFile->open());
    ResponseView view;

    view.setResponse(response, {});

    QCOMPARE(view.property("bodyOnDisk").toBool(), true);
    QCOMPARE(view.property("truncated").toBool(), true);
    QCOMPARE(view.property("canPretty").toBool(), false);
    QCOMPARE(view.property("sizeText").toString(), QStringLiteral("200.0 MB"));
    QCOMPARE(view.property("bodyText").toString(), QStringLiteral("[1, 2"));

    view.setResponse(makeResponse("{}", QStringLiteral("application/json")), {});
    QCOMPARE(view.property("bodyOnDisk").toBool(), false);
}

void ResponseViewTest::certificateCanBeTrustedAfterTlsFailure()
{
    ResponseData failed;
    failed.error = ResponseError::Tls;
    failed.errorString = QStringLiteral("SSL handshake failed");
    failed.tlsChecks = {tlsCheck(QStringLiteral("dev.local"), 8443,
                                 {QStringLiteral("The certificate is self-signed, and untrusted")}, false)};
    failed.finalUrl = QUrl(QStringLiteral("https://dev.local:8443/items"));
    ResponseView view;

    view.setResponse(failed, {});
    QCOMPARE(view.property("canTrustCertificate").toBool(), true);
    QCOMPARE(view.property("certificateHost").toString(), QStringLiteral("dev.local"));

    ResponseData refused = failed;
    refused.error = ResponseError::ConnectionRefused;
    view.setResponse(refused, {});
    QCOMPARE(view.property("canTrustCertificate").toBool(), false);

    ResponseData noMessages = failed;
    noMessages.tlsChecks.clear();
    view.setResponse(noMessages, {});
    QCOMPARE(view.property("canTrustCertificate").toBool(), false);

    view.clear();
    QCOMPARE(view.property("canTrustCertificate").toBool(), false);
}

void ResponseViewTest::ignoredCertificateBecomesWarning()
{
    ResponseData response = makeResponse("{}", QStringLiteral("application/json"));
    response.tlsChecks = {tlsCheck(QStringLiteral("dev.local"), 443,
                                   {QStringLiteral("The certificate is self-signed, and untrusted")}, true)};
    response.finalUrl = QUrl(QStringLiteral("https://dev.local/items"));
    ResponseView view;

    view.setResponse(response, {QStringLiteral("a")});

    const QString warning = view.property("warningText").toString();
    QVERIFY2(warning.contains(QStringLiteral("dev.local")), qPrintable(warning));
    QVERIFY2(warning.contains(QStringLiteral("self-signed")), qPrintable(warning));
    QVERIFY2(warning.contains(QStringLiteral("{{a}}")), qPrintable(warning));
    QCOMPARE(view.property("canTrustCertificate").toBool(), false);
}

void ResponseViewTest::headersModelListsResponseHeaders()
{
    ResponseView view;
    view.setResponse(makeResponse("{}", QStringLiteral("application/json")), {});
    KeyValueModel *model = view.headersModel();
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->rows().at(1).key, QStringLiteral("x-a"));
    QCOMPARE(model->rows().at(1).value, QStringLiteral("1"));
}

void ResponseViewTest::clearResets()
{
    ResponseView view;
    view.setResponse(makeResponse("{}", QStringLiteral("application/json")), {});
    view.clear();

    QVERIFY(!view.property("hasResponse").toBool());
    QVERIFY(view.property("bodyText").toString().isEmpty());
    QCOMPARE(view.headersModel()->rowCount(), 0);
}

QTEST_GUILESS_MAIN(ResponseViewTest)

#include "tst_responseview.moc"
