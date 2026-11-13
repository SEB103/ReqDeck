// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QFile>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
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
    /*! Verifies that the exception does not cover another port of the host. */
    void tlsExceptionCoversOnlyItsPort();
    /*! Verifies that the exception does not cover errors other than those it was made for. */
    void tlsExceptionCoversOnlyItsErrors();

    /*! Verifies that a body past the memory limit goes whole to a file, and its start stays in memory. */
    void largeBodySpillsToFile();
    /*! Verifies that a body within the limit stays in memory without a file. */
    void smallBodyStaysInMemory();
    /*! Verifies that the file goes with the last copy of the response. */
    void spillFileGoesWithResponse();
    /*! Verifies the default memory limit. */
    void defaultBodyMemoryLimit();
    /*! Verifies that a cancel from the busy signal still ends with one finished(). */
    void abortFromBusySignal();
    /*! Verifies that body files left by an earlier run are removed, and nothing else. */
    void staleBodyFilesRemoved();
    /*! Verifies that a body that cannot go to disk is an error that keeps no size or data. */
    void failedSpillKeepsNothing();

    /*! Verifies that requests ask the system for a proxy, as browsers on Windows do. */
    void usesSystemProxy();
    /*! Verifies that the computer itself is always reached directly. */
    void loopbackNeverUsesProxy();
    /*! Provides host names with whether they name this computer. */
    void isThisComputer_data();
    /*! Verifies which host names name this computer. */
    void isThisComputer();

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

    /*! Verifies that with a store, cookies a server set go back to it, two headers at once included. */
    void storedCookiesGoBack();

    /*! Verifies that a cookie of 127.0.0.1 is not sent to localhost, the same server under another name. */
    void cookiesStayWithTheirHost();

    /*! Verifies that a cookie set by a redirect is sent on the next hop. */
    void redirectStoresAndSendsPerHop();

    /*! Verifies that a multipart body with a file arrives whole, with its Content-Length. */
    void sendsMultipartWithFile();

    /*! Verifies that a 307 redirect sends a multipart body again. */
    void redirectResendsMultipartBody();

    /*! Verifies that a large file body arrives whole. */
    void largeFileIsStreamed();

    /*! Verifies that a file removed after building fails the send with its path. */
    void vanishedFileFailsTheSend();

    /*! Verifies that a send that could not start stays busy until its finish, and abort() ends it. */
    void failedStartStaysBusyUntilFinished();

    /*! Verifies that a typed Cookie header is sent as typed, without the stored cookies. */
    void typedCookieHeaderWins();

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

    /*! Verifies that every hop is recorded with its answer, credentials masked. */
    void sentViewMasksCredentials();

    /*! Verifies that an API key in the query and a URL password are masked in the hop. */
    void sentHopMasksQueryKeyAndPassword();

    /*! Verifies that a file body does not follow a 307 to another server. */
    void filesStayOnTheirServer();

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

/*!
 * \internal
 * \brief Sends a request to \a server, which fails its certificate check, and
 * returns that check, as the offer to continue gets it.
 */
static TlsCheck failedCheck(HttpExecutor &executor, MockHttpServer &server)
{
    const auto response = sendAndWait(executor, buildRequest(specFor(server.url(QStringLiteral("/echo")))));
    if (!response || response->tlsChecks.isEmpty())
        return {};
    return response->tlsChecks.last();
}

void HttpExecutorTest::tlsErrorKeepsMessages()
{
    const auto server = startTlsServer();
    QVERIFY(server);
    HttpExecutor executor;

    const auto response = sendAndWait(executor, buildRequest(specFor(server->url(QStringLiteral("/echo")))));

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Tls);
    QCOMPARE(response->tlsChecks.size(), 1);
    const TlsCheck &check = response->tlsChecks.first();
    QCOMPARE(check.host, QStringLiteral("127.0.0.1"));
    QCOMPARE(check.port, server->url(QStringLiteral("/")).port());
    QVERIFY(!check.messages.isEmpty());
    QVERIFY(!check.errors.isEmpty());
    QVERIFY(!check.ignored);
    QVERIFY(!response->tlsErrorsIgnored());
    QCOMPARE(server->requestCount(), 0);
}

void HttpExecutorTest::tlsExceptionCoversItsHost()
{
    const auto server = startTlsServer();
    QVERIFY(server);
    HttpExecutor executor;
    BuiltRequest request = buildRequest(specFor(server->url(QStringLiteral("/echo"))));
    request.tlsException = failedCheck(executor, *server);
    QVERIFY(!request.tlsException.isEmpty());

    const auto response = sendAndWait(executor, request);

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::None);
    QCOMPARE(response->statusCode, 200);
    QVERIFY(response->tlsErrorsIgnored());
    QCOMPARE(response->tlsChecks.size(), 1);
    QVERIFY(response->tlsChecks.first().ignored);
    QVERIFY(!response->tlsChecks.first().messages.isEmpty());
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
    request.tlsException = failedCheck(executor, *server);
    QVERIFY(!request.tlsException.isEmpty());

    const auto response = sendAndWait(executor, request);

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Tls);
    QCOMPARE(server->requestCount(), 1);
    // Each server keeps its own outcome: the first was not checked, the second failed.
    QCOMPARE(response->tlsChecks.size(), 2);
    QCOMPARE(response->tlsChecks.at(0).host, QStringLiteral("127.0.0.1"));
    QVERIFY(response->tlsChecks.at(0).ignored);
    QCOMPARE(response->tlsChecks.at(1).host, QStringLiteral("localhost"));
    QVERIFY(!response->tlsChecks.at(1).ignored);
}

void HttpExecutorTest::tlsExceptionIsOneRequest()
{
    const auto server = startTlsServer();
    QVERIFY(server);
    HttpExecutor executor;
    BuiltRequest trusted = buildRequest(specFor(server->url(QStringLiteral("/echo"))));
    trusted.tlsException = failedCheck(executor, *server);
    const auto first = sendAndWait(executor, trusted);
    QVERIFY(first);
    QCOMPARE(first->statusCode, 200);

    const auto second = sendAndWait(executor, buildRequest(specFor(server->url(QStringLiteral("/echo")))));

    QVERIFY(second);
    QCOMPARE(second->error, ResponseError::Tls);
    QVERIFY(!second->tlsErrorsIgnored());
}

void HttpExecutorTest::tlsExceptionCoversOnlyItsPort()
{
    const auto server = startTlsServer();
    const auto other = startTlsServer();
    QVERIFY(server);
    QVERIFY(other);
    HttpExecutor executor;
    BuiltRequest request = buildRequest(specFor(other->url(QStringLiteral("/echo"))));
    request.tlsException = failedCheck(executor, *server);
    QVERIFY(!request.tlsException.isEmpty());

    const auto response = sendAndWait(executor, request);

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Tls);
    QCOMPARE(other->requestCount(), 0);
}

void HttpExecutorTest::tlsExceptionCoversOnlyItsErrors()
{
    const auto server = startTlsServer();
    QVERIFY(server);
    HttpExecutor executor;
    BuiltRequest request = buildRequest(specFor(server->url(QStringLiteral("/echo"))));
    request.tlsException = failedCheck(executor, *server);
    QVERIFY(!request.tlsException.isEmpty());
    // The same kind of error for no certificate at all: another certificate must not pass.
    request.tlsException.errors = {QSslError(QSslError::SelfSignedCertificate)};

    const auto response = sendAndWait(executor, request);

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Tls);
    QCOMPARE(server->requestCount(), 0);
}

void HttpExecutorTest::largeBodySpillsToFile()
{
    constexpr qint64 kLimit = 1024 * 1024;
    constexpr qint64 kSize = 5 * kLimit + 123;
    HttpExecutor executor;
    executor.setBodyMemoryLimit(kLimit);

    const auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/bytes/%1").arg(kSize)))), 20000);

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::None);
    QCOMPARE(response->sizeBytes, kSize);
    QVERIFY(response->bodyFile);
    QCOMPARE(response->body, MockHttpServer::patternBytes(0, kLimit));
    QFile file(response->bodyFile->fileName());
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.size(), kSize);
    QCOMPARE(file.readAll(), MockHttpServer::patternBytes(0, kSize));
}

void HttpExecutorTest::smallBodyStaysInMemory()
{
    constexpr qint64 kLimit = 1024 * 1024;
    HttpExecutor executor;
    executor.setBodyMemoryLimit(kLimit);

    const auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/bytes/%1").arg(kLimit)))), 20000);

    QVERIFY(response);
    QVERIFY(!response->bodyFile);
    QCOMPARE(response->sizeBytes, kLimit);
    QCOMPARE(response->body, MockHttpServer::patternBytes(0, kLimit));
}

void HttpExecutorTest::spillFileGoesWithResponse()
{
    HttpExecutor executor;
    executor.setBodyMemoryLimit(64 * 1024);
    auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/bytes/300000")))), 20000);
    QVERIFY(response);
    QVERIFY(response->bodyFile);
    const QString path = response->bodyFile->fileName();
    QVERIFY(QFile::exists(path));
    ResponseData copy = *response;

    response.reset();
    QVERIFY(QFile::exists(path));
    copy = ResponseData();

    QVERIFY(!QFile::exists(path));
}

void HttpExecutorTest::defaultBodyMemoryLimit()
{
    const HttpExecutor executor;
    QCOMPARE(executor.bodyMemoryLimit(), qint64(32 * 1024 * 1024));
}

void HttpExecutorTest::abortFromBusySignal()
{
    HttpExecutor executor;
    QList<bool> busy;
    int finished = 0;
    std::optional<ResponseData> result;
    connect(&executor, &HttpExecutor::busyChanged, &executor, [&](bool running) {
        busy.append(running);
        if (running)
            executor.abort();
    });
    connect(&executor, &HttpExecutor::finished, &executor, [&](const ResponseData &response) {
        ++finished;
        result = response;
    });

    QVERIFY(executor.send(buildRequest(specFor(m_server.url(QStringLiteral("/delay/2000"))))));
    QTRY_COMPARE(finished, 1);
    QTest::qWait(100);

    QCOMPARE(finished, 1);
    QCOMPARE(busy, QList<bool>({true, false}));
    QCOMPARE(result->error, ResponseError::Aborted);
    QVERIFY(!executor.isBusy());
}

void HttpExecutorTest::staleBodyFilesRemoved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QDateTime old = QDateTime::currentDateTime().addDays(-2);
    for (const QString &name : {QStringLiteral("ReqDeck-abc123.body"), QStringLiteral("ReqDeck-x.body"),
                                QStringLiteral("ReqDeck-fresh.body"), QStringLiteral("Other-abc.body"),
                                QStringLiteral("ReqDeck-abc.txt")}) {
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        if (name != QLatin1StringView("ReqDeck-fresh.body"))
            QVERIFY(file.setFileTime(old, QFileDevice::FileModificationTime));
    }

    QCOMPARE(HttpExecutor::removeStaleBodyFiles(dir.path()), 2);

    QCOMPARE(QDir(dir.path()).entryList(QDir::Files, QDir::Name),
             QStringList({QStringLiteral("Other-abc.body"), QStringLiteral("ReqDeck-abc.txt"),
                          QStringLiteral("ReqDeck-fresh.body")}));
}

void HttpExecutorTest::failedSpillKeepsNothing()
{
    // Windows takes the temporary folder from TMP, then TEMP; a missing folder
    // makes the spill file fail to open, as a full or locked disk would.
    QTemporaryDir parent;
    const QString missing = parent.filePath(QStringLiteral("missing"));
    const QByteArray oldTmp = qgetenv("TMP");
    const QByteArray oldTemp = qgetenv("TEMP");
    const auto restore = qScopeGuard([&] {
        qputenv("TMP", oldTmp);
        qputenv("TEMP", oldTemp);
    });
    qputenv("TMP", QFile::encodeName(missing));
    qputenv("TEMP", QFile::encodeName(missing));
    HttpExecutor executor;
    executor.setBodyMemoryLimit(64 * 1024);

    const auto response = sendAndWait(
        executor, buildRequest(specFor(m_server.url(QStringLiteral("/bytes/300000")))), 20000);

    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Network);
    QVERIFY2(response->errorString.contains(QStringLiteral("stored on disk")), qPrintable(response->errorString));
    QCOMPARE(response->sizeBytes, 0);
    QVERIFY(response->body.isEmpty());
    QVERIFY(!response->bodyFile);
}

void HttpExecutorTest::usesSystemProxy()
{
    const HttpExecutor executor;
    const QUrl url(QStringLiteral("https://example.com/items"));

    const QList<QNetworkProxy> proxies = executor.proxiesFor(url);
    const QList<QNetworkProxy> system = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(url));

    QVERIFY(!proxies.isEmpty());
    QCOMPARE(proxies.size(), system.size());
    for (qsizetype i = 0; i < proxies.size(); ++i) {
        QCOMPARE(proxies.at(i).type(), system.at(i).type());
        QCOMPARE(proxies.at(i).hostName(), system.at(i).hostName());
        QCOMPARE(proxies.at(i).port(), system.at(i).port());
    }
}

void HttpExecutorTest::isThisComputer_data()
{
    QTest::addColumn<QString>("host");
    QTest::addColumn<bool>("local");

    QTest::newRow("localhost") << "localhost" << true;
    QTest::newRow("upper case") << "LocalHost" << true;
    QTest::newRow("trailing dot") << "localhost." << true;
    QTest::newRow("subdomain") << "api.localhost" << true;
    QTest::newRow("subdomain trailing dot") << "App.Localhost." << true;
    QTest::newRow("ipv4 loopback") << "127.0.0.1" << true;
    QTest::newRow("ipv4 loopback range") << "127.12.0.5" << true;
    QTest::newRow("ipv6 loopback") << "::1" << true;
    QTest::newRow("domain of localhost") << "localhost.example.com" << false;
    QTest::newRow("name ending in localhost") << "mylocalhost" << false;
    QTest::newRow("other") << "example.com" << false;
    QTest::newRow("private address") << "192.168.1.10" << false;
    QTest::newRow("empty") << "" << false;
}

void HttpExecutorTest::isThisComputer()
{
    QFETCH(QString, host);
    QFETCH(bool, local);

    QCOMPARE(HttpExecutor::isThisComputer(host), local);
}

void HttpExecutorTest::loopbackNeverUsesProxy()
{
    const HttpExecutor executor;
    for (const QString &address : {QStringLiteral("http://127.0.0.1:8080/"), QStringLiteral("http://localhost/"),
                                   QStringLiteral("https://LOCALHOST:8443/x"), QStringLiteral("http://[::1]:3000/"),
                                   QStringLiteral("http://127.12.0.5/"), QStringLiteral("http://localhost./"),
                                   QStringLiteral("http://api.localhost:5000/"),
                                   QStringLiteral("http://App.Localhost./")}) {
        const QList<QNetworkProxy> proxies = executor.proxiesFor(QUrl(address));
        QVERIFY2(proxies.size() == 1, qPrintable(address));
        QVERIFY2(proxies.first().type() == QNetworkProxy::NoProxy, qPrintable(address));
    }
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

void HttpExecutorTest::storedCookiesGoBack()
{
    CookieStore store;
    HttpExecutor executor;
    executor.setCookieStore(&store);
    auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/set-cookies")))));
    QVERIFY(response);
    QCOMPARE(store.cookies().size(), 2);

    response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/echo")))));
    QVERIFY(response);
    QCOMPARE(echoedHeader(echoOf(*response), QStringLiteral("Cookie")), QStringList({QStringLiteral("a=1; b=2")}));
    QVERIFY(!response->storedCookiesSkipped);

    executor.setCookieStore(nullptr);
    response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/echo")))));
    QVERIFY(response);
    QVERIFY(echoedHeader(echoOf(*response), QStringLiteral("Cookie")).isEmpty());
}

void HttpExecutorTest::cookiesStayWithTheirHost()
{
    CookieStore store;
    HttpExecutor executor;
    executor.setCookieStore(&store);
    QVERIFY(sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/set-cookie"))))));

    auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/echo"), QStringLiteral("localhost")))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    QVERIFY(echoedHeader(echoOf(*response), QStringLiteral("Cookie")).isEmpty());

    // A redirect from localhost back to 127.0.0.1 picks up the cookie of 127.0.0.1 there.
    const QString target = QString::fromLatin1(QUrl::toPercentEncoding(m_server.url(QStringLiteral("/echo")).toString()));
    response = sendAndWait(executor, buildRequest(specFor(
        m_server.url(QStringLiteral("/redirect-to?url=") + target, QStringLiteral("localhost")))));
    QVERIFY(response);
    QCOMPARE(echoedHeader(echoOf(*response), QStringLiteral("Cookie")), QStringList({QStringLiteral("sid=server")}));
}

void HttpExecutorTest::redirectStoresAndSendsPerHop()
{
    CookieStore store;
    HttpExecutor executor;
    executor.setCookieStore(&store);
    auto response = sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/login")))));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    QCOMPARE(echoedHeader(echoOf(*response), QStringLiteral("Cookie")), QStringList({QStringLiteral("sid=login")}));
}

void HttpExecutorTest::typedCookieHeaderWins()
{
    CookieStore store;
    HttpExecutor executor;
    executor.setCookieStore(&store);
    QVERIFY(sendAndWait(executor, buildRequest(specFor(m_server.url(QStringLiteral("/set-cookie"))))));

    RequestSpec spec = specFor(m_server.url(QStringLiteral("/echo")));
    spec.headers.append({QStringLiteral("Cookie"), QStringLiteral("sid=user"), true});
    auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(echoedHeader(echoOf(*response), QStringLiteral("Cookie")), QStringList({QStringLiteral("sid=user")}));
    QVERIFY(response->storedCookiesSkipped);
    QCOMPARE(store.cookies().at(0).value(), QByteArray("server"));
}

void HttpExecutorTest::sendsMultipartWithFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("note.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("file content");
    file.close();

    RequestSpec spec = specFor(m_server.url(QStringLiteral("/echo")), HttpMethod::Post);
    spec.body.mode = BodyMode::FormData;
    spec.body.parts = {{QStringLiteral("title"), QStringLiteral("hello"), true, false, QString()},
                       {QStringLiteral("doc"), path, true, true, QString()}};
    HttpExecutor executor;
    auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    const QString body = echoOf(*response).value(QStringLiteral("body")).toString();
    QVERIFY2(body.contains(QStringLiteral("name=\"title\"\r\n\r\nhello\r\n")), qPrintable(body));
    QVERIFY(body.contains(QStringLiteral("filename=\"note.txt\"\r\nContent-Type: text/plain\r\n\r\nfile content\r\n")));
    QCOMPARE(echoedHeader(echoOf(*response), QStringLiteral("Content-Length")),
             QStringList({QString::number(body.toUtf8().size())}));
}

void HttpExecutorTest::redirectResendsMultipartBody()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("note.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("again");
    file.close();

    RequestSpec spec = specFor(m_server.url(QStringLiteral("/redirect307")), HttpMethod::Post);
    spec.body.mode = BodyMode::FormData;
    spec.body.parts = {{QStringLiteral("doc"), path, true, true, QString()}};
    HttpExecutor executor;
    auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    const QJsonObject echo = echoOf(*response);
    QCOMPARE(echo.value(QStringLiteral("method")).toString(), QStringLiteral("POST"));
    QVERIFY2(echo.value(QStringLiteral("body")).toString().contains(QStringLiteral("\r\n\r\nagain\r\n")),
             qPrintable(echo.value(QStringLiteral("body")).toString()));
}

void HttpExecutorTest::largeFileIsStreamed()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("big.bin"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QByteArray block = MockHttpServer::patternBytes(0, 1024 * 1024);
    for (int i = 0; i < 24; ++i) {
        file.write(block);
        hash.addData(block);
    }
    file.close();

    RequestSpec spec = specFor(m_server.url(QStringLiteral("/count")), HttpMethod::Put);
    spec.body.mode = BodyMode::File;
    spec.body.filePath = path;
    HttpExecutor executor;
    auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 200);
    const QJsonObject count = QJsonDocument::fromJson(response->body).object();
    QCOMPARE(count.value(QStringLiteral("size")).toDouble(), 24.0 * 1024 * 1024);
    QCOMPARE(count.value(QStringLiteral("contentLength")).toString(), QString::number(24 * 1024 * 1024));
    QCOMPARE(count.value(QStringLiteral("sha256")).toString(), QString::fromLatin1(hash.result().toHex()));
}

void HttpExecutorTest::vanishedFileFailsTheSend()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("gone.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/echo")), HttpMethod::Post);
    spec.body.mode = BodyMode::File;
    spec.body.filePath = path;
    const BuiltRequest built = buildRequest(spec);
    QVERIFY(built.ok);
    QVERIFY(QFile::remove(path));

    HttpExecutor executor;
    auto response = sendAndWait(executor, built);
    QVERIFY(response);
    QCOMPARE(response->error, ResponseError::Network);
    QVERIFY2(response->errorString.contains(QStringLiteral("gone.txt")), qPrintable(response->errorString));
    QVERIFY(!executor.isBusy());
}

void HttpExecutorTest::failedStartStaysBusyUntilFinished()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("gone.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/echo")), HttpMethod::Post);
    spec.body.mode = BodyMode::File;
    spec.body.filePath = path;
    const BuiltRequest built = buildRequest(spec);
    QVERIFY(QFile::remove(path));

    HttpExecutor executor;
    QList<ResponseError> finished;
    connect(&executor, &HttpExecutor::finished, this,
            [&finished](const ResponseData &response) { finished.append(response.error); });
    QVERIFY(executor.send(built));
    QVERIFY(executor.isBusy());
    QVERIFY(!executor.send(buildRequest(specFor(m_server.url(QStringLiteral("/echo"))))));

    executor.abort();

    QCOMPARE(finished, QList<ResponseError>({ResponseError::Aborted}));
    QVERIFY(!executor.isBusy());
    QTest::qWait(50);
    QCOMPARE(finished.size(), 1);
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

void HttpExecutorTest::sentViewMasksCredentials()
{
    RequestSpec spec = specFor(m_server.url(QStringLiteral("/redirect307")), HttpMethod::Post);
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral("hello");
    spec.headers.append({QStringLiteral("Cookie"), QStringLiteral("sid=abc; theme=dark"), true});
    spec.headers.append({QStringLiteral("X-Trace"), QStringLiteral("t1"), true});
    spec.headers.append({QStringLiteral("PRIVATE-TOKEN"), QStringLiteral("secret-typed"), true});
    spec.auth.type = AuthType::Bearer;
    spec.auth.token = QStringLiteral("secret-token");

    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->sent.size(), 2);

    const SentHop &first = response->sent.at(0);
    QCOMPARE(first.method, QStringLiteral("POST"));
    QCOMPARE(first.url, m_server.url(QStringLiteral("/redirect307")));
    QCOMPARE(first.bodySize, 5);
    QCOMPARE(first.statusCode, 307);
    QCOMPARE(response->sent.at(1).url, m_server.url(QStringLiteral("/echo")));
    QCOMPARE(response->sent.at(1).statusCode, 200);

    const auto valueOf = [&first](const QString &name) {
        for (const auto &[headerName, value] : first.headers) {
            if (headerName.compare(name, Qt::CaseInsensitive) == 0)
                return value;
        }
        return QString();
    };
    const QString mask = QStringLiteral("\u2022\u2022\u2022\u2022\u2022\u2022");
    QCOMPARE(valueOf(QStringLiteral("Authorization")), QStringLiteral("Bearer ") + mask);
    QCOMPARE(valueOf(QStringLiteral("Cookie")), QStringLiteral("sid=%1; theme=%1").arg(mask));
    QCOMPARE(valueOf(QStringLiteral("X-Trace")), QStringLiteral("t1"));
    QCOMPARE(valueOf(QStringLiteral("PRIVATE-TOKEN")), mask);
    for (const SentHop &hop : response->sent) {
        for (const auto &[name, value] : hop.headers)
            QVERIFY2(!value.contains(QLatin1StringView("secret")), qPrintable(name));
    }
}

void HttpExecutorTest::sentHopMasksQueryKeyAndPassword()
{
    QUrl url = m_server.url(QStringLiteral("/echo?a=1&access_token=q-secret&b=x%2Fy"));
    url.setUserName(QStringLiteral("user"));
    url.setPassword(QStringLiteral("pw"));
    RequestSpec spec = specFor(url);
    spec.auth.type = AuthType::ApiKey;
    spec.auth.apiKeyLocation = ApiKeyLocation::Query;
    spec.auth.apiKeyName = QStringLiteral("key");
    spec.auth.apiKeyValue = QStringLiteral("k-secret");

    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->sent.size(), 1);
    const QUrl sent = response->sent.at(0).url;
    QVERIFY(!sent.toString().contains(QLatin1StringView("k-secret")));
    QVERIFY(!sent.toString().contains(QLatin1StringView("q-secret")));
    QVERIFY(!sent.toString().contains(QLatin1StringView("pw")));
    QCOMPARE(sent.userName(), QStringLiteral("user"));
    QCOMPARE(QUrlQuery(sent).queryItemValue(QStringLiteral("a")), QStringLiteral("1"));
    QVERIFY(QUrlQuery(sent).hasQueryItem(QStringLiteral("key")));
    // Other items keep the encoding they were sent with.
    QVERIFY(sent.query(QUrl::FullyEncoded).contains(QLatin1StringView("b=x%2Fy")));

    // A token standing alone before the @ is masked too.
    QUrl tokenUrl = m_server.url(QStringLiteral("/echo"));
    tokenUrl.setUserName(QStringLiteral("ghp-secret"));
    const auto tokenResponse = sendAndWait(executor, buildRequest(specFor(tokenUrl)));
    QVERIFY(tokenResponse);
    QVERIFY(!tokenResponse->sent.at(0).url.toString().contains(QLatin1StringView("ghp-secret")));
}

void HttpExecutorTest::filesStayOnTheirServer()
{
    MockHttpServer other;
    QVERIFY(other.start());
    QTemporaryDir dir;
    QFile file(dir.filePath(QStringLiteral("up.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("secret file");
    file.close();
    const QString target = QString::fromLatin1(QUrl::toPercentEncoding(other.url(QStringLiteral("/echo")).toString()));

    RequestSpec spec = specFor(m_server.url(QStringLiteral("/redirect307-to?url=") + target), HttpMethod::Post);
    spec.body.mode = BodyMode::File;
    spec.body.filePath = file.fileName();
    HttpExecutor executor;
    const auto response = sendAndWait(executor, buildRequest(spec));
    QVERIFY(response);
    QCOMPARE(response->statusCode, 307);
    QVERIFY(response->filesNotRedirected);
    QCOMPARE(other.requestCount(), 0);

    // Without files the same redirect is followed.
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral("text");
    const auto text = sendAndWait(executor, buildRequest(spec));
    QVERIFY(text);
    QCOMPARE(text->statusCode, 200);
    QVERIFY(!text->filesNotRedirected);
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
