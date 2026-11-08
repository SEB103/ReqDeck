// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QHttpHeaders>
#include <QtTest>

#include "core/http/httpencoding.h"
#include "core/http/requestbuilder.h"

/*!
 * \internal
 * \brief Returns a GET request spec for \a url.
 */
static RequestSpec specFor(const QString &url, HttpMethod method = HttpMethod::Get)
{
    RequestSpec spec;
    spec.method = method;
    spec.url = url;
    return spec;
}

/*!
 * \internal
 * \brief Returns builder options with a fixed test User-Agent.
 */
static RequestBuilder::Options testOptions()
{
    RequestBuilder::Options options;
    options.userAgent = QStringLiteral("ReqDeck/test");
    return options;
}

/*!
 * \internal
 * \brief Builds \a spec with \a variables and the test options.
 */
static BuiltRequest build(const RequestSpec &spec, const VariableResolver &variables = {})
{
    return RequestBuilder::build(spec, variables, testOptions());
}

/*! Verifies how RequestBuilder turns a RequestSpec into a network request. */
class RequestBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies percent-encoding and filtering of key/value pairs. */
    void encodesPairs();

    /*! Verifies that an empty URL is rejected. */
    void rejectsEmptyUrl();

    /*! Verifies that an unresolved variable in the URL is rejected. */
    void rejectsUnresolvedInUrl();

    /*! Verifies that a URL without scheme defaults to http. */
    void prefixesMissingScheme();

    /*! Verifies that only http and https are accepted. */
    void rejectsUnsupportedScheme();

    /*! Verifies that an invalid URL is reported as typed, so no variable value shows. */
    void invalidUrlKeepsVariables();

    /*! Verifies that parameters extend an existing query string. */
    void appendsParamsToExistingQuery();

    /*! Verifies that repeated header names are merged into one header in order. */
    void mergesDuplicateHeaders();

    /*! Verifies that disabled headers are not sent. */
    void skipsDisabledHeaders();

    /*! Verifies that an invalid header name is rejected. */
    void rejectsInvalidHeaderName();

    /*! Verifies that Basic auth replaces a manual Authorization header. */
    void basicAuthReplacesManualHeader();

    /*! Verifies the Bearer Authorization header. */
    void bearerAuth();

    /*! Verifies API keys sent as header and as query parameter. */
    void apiKeyHeaderAndQuery();

    /*! Verifies the default User-Agent and a manual override. */
    void userAgentDefaultAndOverride();

    /*! Verifies the default Content-Type per body mode. */
    void defaultContentTypes_data();
    /*! Runs one default Content-Type case. */
    void defaultContentTypes();

    /*! Verifies that a manual Content-Type is kept as the only one. */
    void manualContentTypeWins();

    /*! Verifies that HEAD requests never carry a body. */
    void headDropsBody();

    /*! Verifies variable resolution in headers, bodies, and auth. */
    void resolvesVariablesEverywhere();

    /*! Verifies transfer timeout and redirect policy settings. */
    void timeoutAndRedirectPolicy();

    /*! Verifies the headers removed when a redirect leaves the origin. */
    void listsCredentialHeaders();

    /*! Verifies that CR/LF in header values (manual or auth) is rejected. */
    void rejectsInvalidHeaderValue();

    /*! Verifies that non-ASCII header values are sent as UTF-8. */
    void sendsUtf8HeaderValues();

    /*! Verifies that "://" later in the URL does not count as a scheme. */
    void detectsSchemeOnlyAtStart();
};

void RequestBuilderTest::encodesPairs()
{
    const QList<KeyValue> pairs {
        {QStringLiteral("a"), QStringLiteral("1"), true},
        {QStringLiteral("b c"), QStringLiteral("x&y"), true},
        {QStringLiteral("d"), QStringLiteral("2"), false},
        {QString(), QStringLiteral("3"), true},
    };
    QCOMPARE(encodeKeyValuePairs(pairs), QByteArray("a=1&b%20c=x%26y"));
}

void RequestBuilderTest::rejectsEmptyUrl()
{
    const BuiltRequest built = build(specFor(QStringLiteral("   ")));
    QVERIFY(!built.ok);
    QCOMPARE(built.errorString, QStringLiteral("URL is empty."));
}

void RequestBuilderTest::invalidUrlKeepsVariables()
{
    const VariableResolver variables =
        VariableResolver::fromKeyValues({{QStringLiteral("host"), QStringLiteral("[secret-value"), true}});

    const BuiltRequest built = build(specFor(QStringLiteral("http://{{host}}/x")), variables);

    QVERIFY(!built.ok);
    QCOMPARE(built.errorString, QStringLiteral("Invalid URL: http://{{host}}/x"));
}

void RequestBuilderTest::rejectsUnresolvedInUrl()
{
    const BuiltRequest built = build(specFor(QStringLiteral("{{baseUrl}}/x")));
    QVERIFY(!built.ok);
    QVERIFY2(built.errorString.contains(QStringLiteral("{{baseUrl}}")), qPrintable(built.errorString));
    QCOMPARE(built.unresolvedVariables, QStringList({QStringLiteral("baseUrl")}));
}

void RequestBuilderTest::prefixesMissingScheme()
{
    const BuiltRequest built = build(specFor(QStringLiteral("localhost:8080/x")));
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.url(), QUrl(QStringLiteral("http://localhost:8080/x")));
}

void RequestBuilderTest::rejectsUnsupportedScheme()
{
    const BuiltRequest built = build(specFor(QStringLiteral("ftp://h/")));
    QVERIFY(!built.ok);
    QCOMPARE(built.errorString, QStringLiteral("Unsupported URL scheme: ftp"));
}

void RequestBuilderTest::appendsParamsToExistingQuery()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/p?x=1"));
    spec.queryParams.append({QStringLiteral("y"), QStringLiteral("2"), true});
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.url().query(QUrl::FullyEncoded), QStringLiteral("x=1&y=2"));
}

void RequestBuilderTest::mergesDuplicateHeaders()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    spec.headers.append({QStringLiteral("X-Tag"), QStringLiteral("a"), true});
    spec.headers.append({QStringLiteral("Cookie"), QStringLiteral("s=1"), true});
    spec.headers.append({QStringLiteral("x-tag"), QStringLiteral("b"), true});
    spec.headers.append({QStringLiteral("Cookie"), QStringLiteral("t=2"), true});
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    const QHttpHeaders headers = built.request.headers();
    QCOMPARE(headers.values("X-Tag"), QList<QByteArray>({"a, b"}));
    QCOMPARE(headers.values("Cookie"), QList<QByteArray>({"s=1; t=2"}));
}

void RequestBuilderTest::skipsDisabledHeaders()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    spec.headers.append({QStringLiteral("X-Off"), QStringLiteral("1"), false});
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QVERIFY(!built.request.headers().contains("X-Off"));
}

void RequestBuilderTest::rejectsInvalidHeaderName()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    spec.headers.append({QStringLiteral("Bad Name"), QStringLiteral("1"), true});
    const BuiltRequest built = build(spec);
    QVERIFY(!built.ok);
    QCOMPARE(built.errorString, QStringLiteral("Invalid header name: Bad Name"));
}

void RequestBuilderTest::basicAuthReplacesManualHeader()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    spec.headers.append({QStringLiteral("Authorization"), QStringLiteral("x"), true});
    spec.auth.type = AuthType::Basic;
    spec.auth.username = QStringLiteral("user");
    spec.auth.password = QStringLiteral("pass");
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.headers().values("Authorization"),
             QList<QByteArray>({"Basic dXNlcjpwYXNz"}));
}

void RequestBuilderTest::bearerAuth()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    spec.auth.type = AuthType::Bearer;
    spec.auth.token = QStringLiteral("abc");
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.headers().values("Authorization"), QList<QByteArray>({"Bearer abc"}));
}

void RequestBuilderTest::apiKeyHeaderAndQuery()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    spec.headers.append({QStringLiteral("X-Api-Key"), QStringLiteral("old"), true});
    spec.auth.type = AuthType::ApiKey;
    spec.auth.apiKeyName = QStringLiteral("X-Api-Key");
    spec.auth.apiKeyValue = QStringLiteral("k1");
    BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.headers().values("X-Api-Key"), QList<QByteArray>({"k1"}));

    spec.headers.clear();
    spec.auth.apiKeyName = QStringLiteral("key");
    spec.auth.apiKeyLocation = ApiKeyLocation::Query;
    built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.url().query(QUrl::FullyEncoded), QStringLiteral("key=k1"));
    QVERIFY(!built.request.headers().contains("key"));
}

void RequestBuilderTest::userAgentDefaultAndOverride()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    BuiltRequest built = build(spec);
    QCOMPARE(built.request.headers().values("User-Agent"), QList<QByteArray>({"ReqDeck/test"}));

    spec.headers.append({QStringLiteral("User-Agent"), QStringLiteral("curl/8"), true});
    built = build(spec);
    QCOMPARE(built.request.headers().values("User-Agent"), QList<QByteArray>({"curl/8"}));
}

void RequestBuilderTest::defaultContentTypes_data()
{
    QTest::addColumn<BodyMode>("mode");
    QTest::addColumn<RawBodyType>("rawType");
    QTest::addColumn<QByteArray>("contentType");

    QTest::newRow("json") << BodyMode::Raw << RawBodyType::Json << QByteArray("application/json");
    QTest::newRow("xml") << BodyMode::Raw << RawBodyType::Xml << QByteArray("application/xml");
    QTest::newRow("text") << BodyMode::Raw << RawBodyType::Text
                          << QByteArray("text/plain; charset=utf-8");
    QTest::newRow("form") << BodyMode::FormUrlEncoded << RawBodyType::Json
                          << QByteArray("application/x-www-form-urlencoded");
}

void RequestBuilderTest::defaultContentTypes()
{
    QFETCH(BodyMode, mode);
    QFETCH(RawBodyType, rawType);
    QFETCH(QByteArray, contentType);

    RequestSpec spec = specFor(QStringLiteral("http://h/"), HttpMethod::Post);
    spec.body.mode = mode;
    spec.body.rawType = rawType;
    spec.body.raw = QStringLiteral("x");
    spec.body.form.append({QStringLiteral("a"), QStringLiteral("1"), true});
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.headers().values("Content-Type"), QList<QByteArray>({contentType}));
    QCOMPARE(built.body, mode == BodyMode::Raw ? QByteArray("x") : QByteArray("a=1"));
}

void RequestBuilderTest::manualContentTypeWins()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"), HttpMethod::Post);
    spec.headers.append({QStringLiteral("content-type"), QStringLiteral("application/vnd.api+json"), true});
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral("{}");
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.headers().values("Content-Type"),
             QList<QByteArray>({"application/vnd.api+json"}));
}

void RequestBuilderTest::headDropsBody()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"), HttpMethod::Head);
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral("{}");
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.verb, QByteArray("HEAD"));
    QVERIFY(built.body.isEmpty());
    QVERIFY(!built.request.headers().contains("Content-Type"));
}

void RequestBuilderTest::resolvesVariablesEverywhere()
{
    const VariableResolver variables(QHash<QString, QString> {
        {QStringLiteral("host"), QStringLiteral("h")},
        {QStringLiteral("tag"), QStringLiteral("t1")},
        {QStringLiteral("name"), QStringLiteral("Ann")},
        {QStringLiteral("token"), QStringLiteral("secret")},
    });

    RequestSpec spec = specFor(QStringLiteral("http://{{host}}/"), HttpMethod::Post);
    spec.headers.append({QStringLiteral("X-Tag"), QStringLiteral("{{tag}}"), true});
    spec.headers.append({QStringLiteral("X-Missing"), QStringLiteral("{{missing}}"), true});
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral(R"({"name":"{{name}}"})");
    spec.auth.type = AuthType::Bearer;
    spec.auth.token = QStringLiteral("{{token}}");

    BuiltRequest built = build(spec, variables);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.url(), QUrl(QStringLiteral("http://h/")));
    QCOMPARE(built.request.headers().values("X-Tag"), QList<QByteArray>({"t1"}));
    QCOMPARE(built.request.headers().values("X-Missing"), QList<QByteArray>({"{{missing}}"}));
    QCOMPARE(built.request.headers().values("Authorization"), QList<QByteArray>({"Bearer secret"}));
    QCOMPARE(built.body, QByteArray(R"({"name":"Ann"})"));
    QCOMPARE(built.unresolvedVariables, QStringList({QStringLiteral("missing")}));

    spec.body.mode = BodyMode::FormUrlEncoded;
    spec.body.form.append({QStringLiteral("who"), QStringLiteral("{{name}}"), true});
    built = build(spec, variables);
    QCOMPARE(built.body, QByteArray("who=Ann"));
}

void RequestBuilderTest::timeoutAndRedirectPolicy()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    BuiltRequest built = build(spec);
    QCOMPARE(built.request.transferTimeoutAsDuration(), std::chrono::milliseconds(30000));
    QVERIFY(built.followRedirects);
    // Redirects are followed by HttpExecutor itself, never by Qt.
    QCOMPARE(built.request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
             int(QNetworkRequest::ManualRedirectPolicy));

    spec.timeoutMs = 1500;
    RequestBuilder::Options options = testOptions();
    options.followRedirects = false;
    built = RequestBuilder::build(spec, {}, options);
    QCOMPARE(built.request.transferTimeoutAsDuration(), std::chrono::milliseconds(1500));
    QVERIFY(!built.followRedirects);
    QCOMPARE(built.request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
             int(QNetworkRequest::ManualRedirectPolicy));
}

void RequestBuilderTest::listsCredentialHeaders()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    BuiltRequest built = build(spec);
    QCOMPARE(built.credentialHeaders,
             QList<QByteArray>({"Authorization", "Proxy-Authorization", "Cookie"}));

    spec.auth.type = AuthType::ApiKey;
    spec.auth.apiKeyName = QStringLiteral("X-Api-Key");
    spec.auth.apiKeyValue = QStringLiteral("k1");
    built = build(spec);
    QCOMPARE(built.credentialHeaders,
             QList<QByteArray>({"Authorization", "Proxy-Authorization", "Cookie", "X-Api-Key"}));
}

void RequestBuilderTest::rejectsInvalidHeaderValue()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    spec.headers.append({QStringLiteral("X-A"), QStringLiteral("a\r\nInjected: 1"), true});
    BuiltRequest built = build(spec);
    QVERIFY(!built.ok);
    QCOMPARE(built.errorString, QStringLiteral("Invalid value for header: X-A"));

    spec.headers.clear();
    spec.auth.type = AuthType::Bearer;
    spec.auth.token = QStringLiteral("abc\n");
    built = build(spec);
    QVERIFY(!built.ok);
    QCOMPARE(built.errorString, QStringLiteral("Invalid value for header: Authorization"));
}

void RequestBuilderTest::sendsUtf8HeaderValues()
{
    RequestSpec spec = specFor(QStringLiteral("http://h/"));
    spec.headers.append({QStringLiteral("X-Name"), QStringLiteral("Müller €"), true});
    const BuiltRequest built = build(spec);
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.headers().values("X-Name"),
             QList<QByteArray>({QStringLiteral("Müller €").toUtf8()}));
}

void RequestBuilderTest::detectsSchemeOnlyAtStart()
{
    const BuiltRequest built = build(specFor(QStringLiteral("localhost:3000/cb?next=http://x")));
    QVERIFY2(built.ok, qPrintable(built.errorString));
    QCOMPARE(built.request.url().host(), QStringLiteral("localhost"));
    QCOMPARE(built.request.url().port(), 3000);
    QCOMPARE(built.request.url().scheme(), QStringLiteral("http"));
}

QTEST_GUILESS_MAIN(RequestBuilderTest)

#include "tst_requestbuilder.moc"
