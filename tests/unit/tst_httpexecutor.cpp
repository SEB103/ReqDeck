// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTimer>
#include <QtTest>

#include <memory>
#include <optional>

#include "core/http/httpexecutor.h"
#include "core/http/requestbuilder.h"
#include "mockhttpserver.h"

/*!
 * \internal
 * \brief Builds \a spec with the test User-Agent and the given redirect policy.
 */
static BuiltRequest buildRequest(const RequestSpec &spec, bool followRedirects = true)
{
    RequestBuilder::Options options;
    options.userAgent = QStringLiteral("ReqDeck/test");
    options.followRedirects = followRedirects;
    return RequestBuilder::build(spec, {}, options);
}

/*!
 * \internal
 * \brief Returns a request spec for \a method on \a url.
 */
static RequestSpec specFor(const QUrl &url, HttpMethod method = HttpMethod::Get)
{
    RequestSpec spec;
    spec.method = method;
    spec.url = url.toString();
    return spec;
}

/*!
 * \internal
 * \brief Sends \a request and waits up to \a timeoutMs for HttpExecutor::finished.
 * \return The response, or std::nullopt when send() refused or nothing arrived.
 */
static std::optional<ResponseData> sendAndWait(HttpExecutor &executor, const BuiltRequest &request,
                                               int timeoutMs = 5000)
{
    std::optional<ResponseData> result;
    QEventLoop loop;
    const QMetaObject::Connection connection = QObject::connect(
        &executor, &HttpExecutor::finished, &loop, [&](const ResponseData &response) {
            result = response;
            loop.quit();
        });
    if (executor.send(request) && !result) {
        QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
        loop.exec();
    }
    QObject::disconnect(connection);
    return result;
}

/*!
 * \internal
 * \brief Parses the JSON body of an /echo response.
 */
static QJsonObject echoOf(const ResponseData &response)
{
    return QJsonDocument::fromJson(response.body).object();
}

/*!
 * \internal
 * \brief Returns all values of header \a name (case-insensitive) recorded by /echo.
 */
static QStringList echoedHeader(const QJsonObject &echo, const QString &name)
{
    QStringList values;
    for (const QJsonValue &entry : echo.value(QStringLiteral("headers")).toArray()) {
        const QJsonArray pair = entry.toArray();
        if (pair.at(0).toString().compare(name, Qt::CaseInsensitive) == 0)
            values.append(pair.at(1).toString());
    }
    return values;
}

/*!
 * \internal
 * \brief Returns all values of response header \a name (case-insensitive).
 */
static QList<QByteArray> responseHeader(const ResponseData &response, const QByteArray &name)
{
    QList<QByteArray> values;
    for (const auto &[headerName, value] : response.headers) {
        if (headerName.compare(name, Qt::CaseInsensitive) == 0)
            values.append(value);
    }
    return values;
}

/*! Verifies HttpExecutor against the local MockHttpServer. */
class HttpExecutorTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Starts the mock server shared by all test functions. */
    void initTestCase();

    /*! Verifies that every supported method reaches the server. */
    void sendsEveryMethod_data();
    /*! Runs one method case. */
    void sendsEveryMethod();

    /*! Verifies that headers (repeated names merged), body, auth, and User-Agent arrive. */
    void deliversHeadersBodyAndAuth();

    /*! Verifies byte-exact transport of special characters in query and form. */
    void queryAndFormEncodingSurviveTransport();

    /*! Verifies that HTTP error statuses are responses, not transport errors. */
    void statusCodesAreResponsesNotErrors_data();
    /*! Runs one status case. */
    void statusCodesAreResponsesNotErrors();

    /*! Verifies that an untrusted certificate fails the request and keeps the messages. */
    void tlsErrorKeepsMessages();
    /*! Verifies that an exception for the host lets the request through and records the messages. */
    void tlsExceptionCoversItsHost();
    /*! Verifies that the exception does not reach another host, even by a redirect. */
    void tlsExceptionCoversOnlyItsHost();
    /*! Verifies that the exception ends with its request. */
    void tlsExceptionIsOneRequest();

    /*! Provides statuses whose server sends no reason phrase. */
    void reasonPhraseFilledWhenMissing_data();
    /*! Verifies that a status without reason phrase gets the standard one, as HTTP/2 needs. */
    void reasonPhraseFilledWhenMissing();

    /*! Verifies empty bodies of 204 and HEAD responses. */
    void headAndNoContentReturnEmptyBody();

    /*! Verifies that redirects are followed when enabled. */
    void followsRedirectWhenEnabled();

    /*! Verifies that the redirect response is returned when following is disabled. */
    void returnsRedirectWhenDisabled();

    /*! Verifies the transfer timeout. */
    void timesOut();

    /*! Verifies cancelling a request in flight. */
    void abortsInFlightRequest();

    /*! Verifies that a second send() is refused while a request is running. */
    void rejectsSendWhileBusy();

    /*! Verifies that abort() without a running request does nothing. */
    void abortWhenIdleIsNoOp();

    /*! Verifies that destroying a busy executor emits nothing and does not crash. */
    void destroyWhileInFlightEmitsNothing();

    /*! Verifies that an unbuildable request is refused. */
    void rejectsInvalidBuiltRequest();

    /*! Verifies the error reported for a closed port. */
    void reportsConnectionRefused();

    /*! Verifies that repeated response headers arrive merged in order. */
    void reportsDuplicateResponseHeaders();

    /*! Verifies content type, size, and timing of a response. */
    void reportsTimingAndSize();

    /*! Verifies the mapping from Qt network errors to ResponseError. */
    void classifyErrorMapsNetworkErrors_data();
    /*! Runs one error-mapping case. */
    void classifyErrorMapsNetworkErrors();

    /*! Verifies that cookies set by a server are never stored or sent again. */
    void cookiesAreNeverStored();

    /*! Verifies that credentials are dropped when a redirect leaves the origin. */
    void crossOriginRedirectDropsCredentials();

    /*! Verifies that credentials are kept on a same-origin redirect. */
    void sameOriginRedirectKeepsCredentials();

    /*! Verifies that a 302 after POST continues as GET without a body. */
    void redirectRewritesPostToGet();

    /*! Verifies that a 307 keeps the method and the body. */
    void redirect307KeepsMethodAndBody();

    /*! Verifies that an endless redirect chain ends with an error. */
    void tooManyRedirectsIsAnError();

    /*! Verifies that a body cut off by the server is reported as an error. */
    void truncatedBodyIsAnError();

    /*! Verifies that a finished() slot can start the next request. */
    void sendFromFinishedSlotWorks();

    /*! Verifies that abort() from a finished() slot has no effect. */
    void abortFromFinishedSlotIsNoOp();

    /*! Verifies that non-ASCII header values arrive as UTF-8. */
    void utf8HeaderValueArrives();

private:
    /*! Server shared by all test functions. */
    MockHttpServer m_server;
};

void HttpExecutorTest::initTestCase()
{
    QVERIFY(m_server.start());
}

void HttpExecutorTest::sendsEveryMethod_data()
{
    QTest::addColumn<HttpMethod>("method");

    QTest::newRow("GET") << HttpMethod::Get;
    QTest::newRow("POST") << HttpMethod::Post;
    QTest::newRow("PUT") << HttpMethod::Put;
    QTest::newRow("PATCH") << HttpMethod::Patch;
    QTest::newRow("DELETE") << HttpMethod::Delete;
    QTest::newRow("HEAD") << HttpMethod::Head;
    QTest::newRow("OPTIONS") << HttpMethod::Options;
}

void HttpExecutorTest::sendsEveryMethod()
{
    QFETCH(HttpMethod, method);

    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/echo")), method)));
    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::None);
    QCOMPARE(response->statusCode, 200);
    if (method == HttpMethod::Head) {
        QVERIFY(response->body.isEmpty());
        return;
    }
    QCOMPARE(echoOf(*response).value(QStringLiteral("method")).toString(),
             QString::fromLatin1(httpMethodVerb(method)));
}

void HttpExecutorTest::deliversHeadersBodyAndAuth()
{
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/echo")), HttpMethod::Post);
    spec.headers.append({QStringLiteral("X-Tag"), QStringLiteral("a"), true});
    spec.headers.append({QStringLiteral("X-Tag"), QStringLiteral("b"), true});
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral(R"({"a":1})");
    spec.auth.type = AuthType::Basic;
    spec.auth.username = QStringLiteral("user");
    spec.auth.password = QStringLiteral("pass");

    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    const QJsonObject echo = echoOf(*response);
    QCOMPARE(echoedHeader(echo, QStringLiteral("X-Tag")), QStringList({QStringLiteral("a, b")}));
    QCOMPARE(echoedHeader(echo, QStringLiteral("Content-Type")), QStringList({QStringLiteral("application/json")}));
    QCOMPARE(echoedHeader(echo, QStringLiteral("Authorization")),
             QStringList({QStringLiteral("Basic dXNlcjpwYXNz")}));
    QCOMPARE(echoedHeader(echo, QStringLiteral("User-Agent")), QStringList({QStringLiteral("ReqDeck/test")}));
    QCOMPARE(echo.value(QStringLiteral("body")).toString(), QStringLiteral(R"({"a":1})"));
}

void HttpExecutorTest::queryAndFormEncodingSurviveTransport()
{
    const QString value = QStringLiteral("a&b=c+d e#f ü");
    const QString encoded = QStringLiteral("q=a%26b%3Dc%2Bd%20e%23f%20%C3%BC");

    RequestSpec spec = specFor(m_server.url(QStringLiteral("/echo")));
    spec.queryParams.append({QStringLiteral("q"), value, true});
    HttpExecutor executor;
    auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(echoOf(*response).value(QStringLiteral("query")).toString(), encoded);

    spec = specFor(m_server.url(QStringLiteral("/echo")), HttpMethod::Post);
    spec.body.mode = BodyMode::FormUrlEncoded;
    spec.body.form.append({QStringLiteral("q"), value, true});
    response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(echoOf(*response).value(QStringLiteral("body")).toString(), encoded);
}

void HttpExecutorTest::statusCodesAreResponsesNotErrors_data()
{
    QTest::addColumn<int>("status");

    QTest::newRow("201") << 201;
    QTest::newRow("404") << 404;
    QTest::newRow("500") << 500;
}

void HttpExecutorTest::statusCodesAreResponsesNotErrors()
{
    QFETCH(int, status);

    HttpExecutor executor;
    const auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/status/%1").arg(status)))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, status);
    QCOMPARE(response->error, ResponseError::None);
    QVERIFY(response->errorString.isEmpty());
    QVERIFY(!response->reasonPhrase.isEmpty());
}

/*!
 * \internal
 * \brief Returns an HTTPS mock server, started, or fails the calling test.
 */
static std::unique_ptr<MockHttpServer> startTlsServer()
{
    auto server = std::make_unique<MockHttpServer>();
    if (!server->startTls())
        return {};
    return server;
}

void HttpExecutorTest::tlsErrorKeepsMessages()
{
    const auto server = startTlsServer();
    QVERIFY(server);
    HttpExecutor executor;

    const auto response = sendAndWait(executor, buildRequest(specFor(server->url(QStringLiteral("/echo")))));

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Tls);
    QVERIFY(!response->tlsErrors.isEmpty());
    QVERIFY(!response->tlsErrorsIgnored);
    QCOMPARE(server->requestCount(), 0);
}

void HttpExecutorTest::tlsExceptionCoversItsHost()
{
    const auto server = startTlsServer();
    QVERIFY(server);
    HttpExecutor executor;
    BuiltRequest request = buildRequest(specFor(server->url(QStringLiteral("/echo"))));
    request.tlsExceptionHost = QStringLiteral("127.0.0.1");

    const auto response = sendAndWait(executor, request);

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::None);
    QCOMPARE(response->statusCode, 200);
    QVERIFY(response->tlsErrorsIgnored);
    QVERIFY(!response->tlsErrors.isEmpty());
}

void HttpExecutorTest::tlsExceptionCoversOnlyItsHost()
{
    const auto server = startTlsServer();
    QVERIFY(server);
    HttpExecutor executor;
    const QUrl elsewhere = server->url(QStringLiteral("/echo"), QStringLiteral("localhost"));
    const QString redirect = QStringLiteral("/redirect-to?url=")
                             + QString::fromLatin1(QUrl::toPercentEncoding(elsewhere.toString()));
    BuiltRequest request = buildRequest(specFor(server->url(redirect)));
    request.tlsExceptionHost = QStringLiteral("127.0.0.1");

    const auto response = sendAndWait(executor, request);

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Tls);
    QCOMPARE(server->requestCount(), 1);
}

void HttpExecutorTest::tlsExceptionIsOneRequest()
{
    const auto server = startTlsServer();
    QVERIFY(server);
    HttpExecutor executor;
    BuiltRequest trusted = buildRequest(specFor(server->url(QStringLiteral("/echo"))));
    trusted.tlsExceptionHost = QStringLiteral("127.0.0.1");
    const auto first = sendAndWait(executor, trusted);
    QVERIFY(first);
    QCOMPARE(first->statusCode, 200);

    const auto second = sendAndWait(executor, buildRequest(specFor(server->url(QStringLiteral("/echo")))));

    QVERIFY(second);
    QCOMPARE(second->error, ResponseError::Tls);
    QVERIFY(!second->tlsErrorsIgnored);
}

void HttpExecutorTest::reasonPhraseFilledWhenMissing_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QString>("phrase");

    QTest::newRow("200") << 200 << QStringLiteral("OK");
    QTest::newRow("201") << 201 << QStringLiteral("Created");
    QTest::newRow("404") << 404 << QStringLiteral("Not Found");
    QTest::newRow("418") << 418 << QStringLiteral("I'm a teapot");
    QTest::newRow("429") << 429 << QStringLiteral("Too Many Requests");
    QTest::newRow("503") << 503 << QStringLiteral("Service Unavailable");
    QTest::newRow("599 unknown") << 599 << QString();
}

void HttpExecutorTest::reasonPhraseFilledWhenMissing()
{
    QFETCH(int, status);
    QFETCH(QString, phrase);

    HttpExecutor executor;
    const auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/no-reason/%1").arg(status)))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, status);
    QCOMPARE(response->reasonPhrase, phrase);
}

void HttpExecutorTest::headAndNoContentReturnEmptyBody()
{
    HttpExecutor executor;
    auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/status/204")))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 204);
    QVERIFY(response->body.isEmpty());
    QCOMPARE(response->sizeBytes, 0);
    QCOMPARE(response->error, ResponseError::None);

    response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/echo")), HttpMethod::Head)));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    QVERIFY(response->body.isEmpty());
    QCOMPARE(response->sizeBytes, 0);
    QCOMPARE(response->error, ResponseError::None);
}

void HttpExecutorTest::followsRedirectWhenEnabled()
{
    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/redirect")))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    QCOMPARE(response->finalUrl.path(), QStringLiteral("/echo"));
}

void HttpExecutorTest::returnsRedirectWhenDisabled()
{
    HttpExecutor executor;
    const auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/redirect"))), false));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 302);
    QCOMPARE(responseHeader(*response, "Location"), QList<QByteArray>({"/echo"}));
    QCOMPARE(response->error, ResponseError::None);
}

void HttpExecutorTest::timesOut()
{
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/delay/3000")));
    spec.timeoutMs = 200;
    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Timeout);
    QCOMPARE(response->statusCode, 0);
    QVERIFY2(response->elapsedMs >= 150 && response->elapsedMs < 2500,
             qPrintable(QString::number(response->elapsedMs)));
}

void HttpExecutorTest::abortsInFlightRequest()
{
    HttpExecutor executor;
    QList<bool> busyStates;
    connect(&executor, &HttpExecutor::busyChanged, this, [&](bool busy) { busyStates.append(busy); });
    int finishedCount = 0;
    connect(&executor, &HttpExecutor::finished, this, [&](const ResponseData &) { ++finishedCount; });

    QTimer::singleShot(100, &executor, &HttpExecutor::abort);
    const auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/delay/3000")))));
    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Aborted);
    QVERIFY(!executor.isBusy());
    QTest::qWait(200);
    QCOMPARE(finishedCount, 1);
    QCOMPARE(busyStates, QList<bool>({true, false}));
}

void HttpExecutorTest::rejectsSendWhileBusy()
{
    HttpExecutor executor;
    const BuiltRequest echo = buildRequest(specFor(m_server.url(QStringLiteral("/echo"))));
    QTimer::singleShot(0, &executor, [&]() { QVERIFY(!executor.send(echo)); });
    const auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/delay/300")))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    QCOMPARE(response->body, QByteArray("delayed"));
}

void HttpExecutorTest::abortWhenIdleIsNoOp()
{
    HttpExecutor executor;
    int finishedCount = 0;
    connect(&executor, &HttpExecutor::finished, this, [&](const ResponseData &) { ++finishedCount; });
    executor.abort();
    QTest::qWait(200);
    QCOMPARE(finishedCount, 0);
    QVERIFY(!executor.isBusy());
}

void HttpExecutorTest::destroyWhileInFlightEmitsNothing()
{
    auto executor = std::make_unique<HttpExecutor>();
    int finishedCount = 0;
    connect(executor.get(), &HttpExecutor::finished, this, [&](const ResponseData &) { ++finishedCount; });
    QVERIFY(executor->send(buildRequest(specFor(m_server.url(QStringLiteral("/delay/3000"))))));
    QTest::qWait(100);
    executor.reset();
    QTest::qWait(200);
    QCOMPARE(finishedCount, 0);
}

void HttpExecutorTest::rejectsInvalidBuiltRequest()
{
    HttpExecutor executor;
    QVERIFY(!executor.send(BuiltRequest {}));
    QVERIFY(!executor.isBusy());
}

void HttpExecutorTest::reportsConnectionRefused()
{
    QTcpServer closed;
    QVERIFY(closed.listen(QHostAddress::LocalHost, 0));
    const quint16 port = closed.serverPort();
    closed.close();

    HttpExecutor executor;
    const auto response = sendAndWait(
        executor, buildRequest(specFor(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(port)))), 10000);
    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::ConnectionRefused);
    QVERIFY(!response->errorString.isEmpty());
}

void HttpExecutorTest::reportsDuplicateResponseHeaders()
{
    HttpExecutor executor;
    const auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/response-headers")))));
    QVERIFY(response);
    QCOMPARE(responseHeader(*response, "X-Dup"), QList<QByteArray>({"a, b"}));
}

void HttpExecutorTest::reportsTimingAndSize()
{
    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/echo")))));
    QVERIFY(response);
    QVERIFY(response->contentType.startsWith(QStringLiteral("application/json")));
    QVERIFY(!response->body.isEmpty());
    QCOMPARE(response->sizeBytes, response->body.size());
    QVERIFY(response->elapsedMs >= 0);
    QCOMPARE(response->finalUrl, m_server.url(QStringLiteral("/echo")));
}

void HttpExecutorTest::classifyErrorMapsNetworkErrors_data()
{
    QTest::addColumn<QNetworkReply::NetworkError>("error");
    QTest::addColumn<bool>("abortRequested");
    QTest::addColumn<ResponseError>("expected");

    QTest::newRow("none") << QNetworkReply::NoError << false << ResponseError::None;
    QTest::newRow("cancel-timeout") << QNetworkReply::OperationCanceledError << false << ResponseError::Timeout;
    QTest::newRow("cancel-abort") << QNetworkReply::OperationCanceledError << true << ResponseError::Aborted;
    QTest::newRow("timeout") << QNetworkReply::TimeoutError << false << ResponseError::Timeout;
    QTest::newRow("refused") << QNetworkReply::ConnectionRefusedError << false
                             << ResponseError::ConnectionRefused;
    QTest::newRow("host") << QNetworkReply::HostNotFoundError << false << ResponseError::HostNotFound;
    QTest::newRow("tls") << QNetworkReply::SslHandshakeFailedError << false << ResponseError::Tls;
    QTest::newRow("other") << QNetworkReply::RemoteHostClosedError << false << ResponseError::Network;
}

void HttpExecutorTest::classifyErrorMapsNetworkErrors()
{
    QFETCH(QNetworkReply::NetworkError, error);
    QFETCH(bool, abortRequested);
    QFETCH(ResponseError, expected);

    QCOMPARE(HttpExecutor::classifyError(error, abortRequested), expected);
}

void HttpExecutorTest::cookiesAreNeverStored()
{
    HttpExecutor executor;
    auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/set-cookie")))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);

    RequestSpec spec = specFor(m_server.url(QStringLiteral("/echo")));
    spec.headers.append({QStringLiteral("Cookie"), QStringLiteral("sid=user"), true});
    response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(echoedHeader(echoOf(*response), QStringLiteral("Cookie")), QStringList({QStringLiteral("sid=user")}));

    response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/echo")))));
    QVERIFY(response);
    QVERIFY(echoedHeader(echoOf(*response), QStringLiteral("Cookie")).isEmpty());
}

void HttpExecutorTest::crossOriginRedirectDropsCredentials()
{
    MockHttpServer other;
    QVERIFY(other.start());
    const QString target = QString::fromLatin1(
        QUrl::toPercentEncoding(other.url(QStringLiteral("/echo")).toString()));

    RequestSpec spec = specFor(m_server.url(QStringLiteral("/redirect-to?url=") + target));
    spec.headers.append({QStringLiteral("Cookie"), QStringLiteral("sid=1"), true});
    spec.headers.append({QStringLiteral("X-Trace"), QStringLiteral("t1"), true});
    spec.auth.type = AuthType::Bearer;
    spec.auth.token = QStringLiteral("secret");

    HttpExecutor executor;
    auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    QCOMPARE(response->finalUrl, other.url(QStringLiteral("/echo")));
    QJsonObject echo = echoOf(*response);
    QVERIFY(echoedHeader(echo, QStringLiteral("Authorization")).isEmpty());
    QVERIFY(echoedHeader(echo, QStringLiteral("Cookie")).isEmpty());
    QCOMPARE(echoedHeader(echo, QStringLiteral("X-Trace")), QStringList({QStringLiteral("t1")}));

    spec.auth.type = AuthType::ApiKey;
    spec.auth.apiKeyName = QStringLiteral("X-Api-Key");
    spec.auth.apiKeyValue = QStringLiteral("k1");
    response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    echo = echoOf(*response);
    QVERIFY(echoedHeader(echo, QStringLiteral("X-Api-Key")).isEmpty());
}

void HttpExecutorTest::sameOriginRedirectKeepsCredentials()
{
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/redirect")));
    spec.auth.type = AuthType::Bearer;
    spec.auth.token = QStringLiteral("secret");

    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    QCOMPARE(echoedHeader(echoOf(*response), QStringLiteral("Authorization")),
             QStringList({QStringLiteral("Bearer secret")}));
}

void HttpExecutorTest::redirectRewritesPostToGet()
{
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/redirect")), HttpMethod::Post);
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral("{}");

    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    const QJsonObject echo = echoOf(*response);
    QCOMPARE(echo.value(QStringLiteral("method")).toString(), QStringLiteral("GET"));
    QVERIFY(echo.value(QStringLiteral("body")).toString().isEmpty());
    QVERIFY(echoedHeader(echo, QStringLiteral("Content-Type")).isEmpty());
}

void HttpExecutorTest::redirect307KeepsMethodAndBody()
{
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/redirect307")), HttpMethod::Put);
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral(R"({"a":1})");

    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    const QJsonObject echo = echoOf(*response);
    QCOMPARE(echo.value(QStringLiteral("method")).toString(), QStringLiteral("PUT"));
    QCOMPARE(echo.value(QStringLiteral("body")).toString(), QStringLiteral(R"({"a":1})"));
}

void HttpExecutorTest::tooManyRedirectsIsAnError()
{
    HttpExecutor executor;
    const auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/redirect-loop")))));
    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Network);
    QCOMPARE(response->errorString, QStringLiteral("Too many redirects."));
}

void HttpExecutorTest::truncatedBodyIsAnError()
{
    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/truncated")))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    QCOMPARE(response->error, ResponseError::Network);
    QVERIFY(!response->errorString.isEmpty());
}

void HttpExecutorTest::sendFromFinishedSlotWorks()
{
    HttpExecutor executor;
    const BuiltRequest next = buildRequest(specFor(m_server.url(QStringLiteral("/status/201"))));
    QList<int> statuses;
    connect(&executor, &HttpExecutor::finished, this, [&](const ResponseData &response) {
        statuses.append(response.statusCode);
        if (statuses.size() == 1)
            QVERIFY(executor.send(next));
    });
    QVERIFY(executor.send(buildRequest(specFor(m_server.url(QStringLiteral("/echo"))))));
    QTRY_COMPARE_WITH_TIMEOUT(statuses, QList<int>({200, 201}), 5000);
    QVERIFY(!executor.isBusy());
}

void HttpExecutorTest::abortFromFinishedSlotIsNoOp()
{
    HttpExecutor executor;
    int finishedCount = 0;
    connect(&executor, &HttpExecutor::finished, this, [&](const ResponseData &) {
        ++finishedCount;
        executor.abort();
    });
    QVERIFY(executor.send(buildRequest(specFor(m_server.url(QStringLiteral("/echo"))))));
    QTRY_COMPARE_WITH_TIMEOUT(finishedCount, 1, 5000);
    QTest::qWait(200);
    QCOMPARE(finishedCount, 1);
}

void HttpExecutorTest::utf8HeaderValueArrives()
{
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/echo")));
    spec.headers.append({QStringLiteral("X-Name"), QStringLiteral("Müller €"), true});
    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(echoedHeader(echoOf(*response), QStringLiteral("X-Name")),
             QStringList({QStringLiteral("Müller €")}));
}

QTEST_GUILESS_MAIN(HttpExecutorTest)

#include "tst_httpexecutor.moc"
