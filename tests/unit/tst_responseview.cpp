// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
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
    view.setBusy(QStringLiteral("List users"));

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

void ResponseViewTest::transportErrorShowsMessageAndTlsDetails()
{
    ResponseData response;
    response.error = ResponseError::Tls;
    response.errorString = QStringLiteral("Handshake failed");
    response.tlsErrors = {QStringLiteral("Expired"), QStringLiteral("Wrong host")};

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
