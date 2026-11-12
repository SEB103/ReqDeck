// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QtTest>

#include <optional>

#include "core/http/oauthclient.h"
#include "mockhttpserver.h"

/*! Verifies the OAuth 2.0 client against the mock token endpoint. */
class OAuthClientTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Starts the mock server once. */
    void initTestCase();

    /*! Verifies the PKCE verifier and the S256 challenge of RFC 7636 appendix B. */
    void pkceMatchesTheRfc();

    /*! Verifies client credentials, including a secret that needs encoding. */
    void clientCredentialsGetAToken();

    /*! Verifies the authorization code flow: URL, callback with state, exchange with the verifier. */
    void authorizationCodeWithPkce();

    /*! Verifies that a callback with another state is ignored and cancel ends the flow. */
    void callbackChecksState();

    /*! Verifies that an error from the browser or the token endpoint is reported. */
    void errorsAreReported();

    /*! Verifies refresh and token expiry. */
    void refreshAndExpiry();

private:
    /*! Waits for finished() of \a client and returns its result. */
    static std::optional<OAuthResult> waitFor(OAuthClient &client);

    /*! Settings against the mock server. */
    OAuth2Settings settings() const;

    /*! Mock HTTP server with the token endpoint. */
    MockHttpServer m_server;
};

void OAuthClientTest::initTestCase()
{
    QVERIFY(m_server.start());
}

std::optional<OAuthResult> OAuthClientTest::waitFor(OAuthClient &client)
{
    std::optional<OAuthResult> result;
    QObject::connect(&client, &OAuthClient::finished, &client, [&result](const OAuthResult &r) { result = r; });
    (void)QTest::qWaitFor([&result]() { return result.has_value(); }, 10000);
    if (!result)
        result = OAuthResult {false, QStringLiteral("no finished() within 10 s"), {}};
    return result;
}

OAuth2Settings OAuthClientTest::settings() const
{
    OAuth2Settings s;
    s.authUrl = QStringLiteral("https://id.test/authorize?prompt=login");
    s.tokenUrl = m_server.url(QStringLiteral("/oauth/token")).toString();
    s.clientId = QStringLiteral("app");
    s.scope = QStringLiteral("read write");
    return s;
}

void OAuthClientTest::pkceMatchesTheRfc()
{
    QCOMPARE(OAuthClient::codeChallenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
             QByteArray("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));
    const QByteArray verifier = OAuthClient::codeVerifier();
    QCOMPARE(verifier.size(), 64);
    QVERIFY(QRegularExpression(QStringLiteral("^[A-Za-z0-9._~-]+$")).match(QString::fromLatin1(verifier)).hasMatch());
    QVERIFY(OAuthClient::codeVerifier() != verifier);
}

void OAuthClientTest::clientCredentialsGetAToken()
{
    OAuthClient client;
    OAuth2Settings s = settings();
    s.grant = OAuthGrant::ClientCredentials;
    s.clientSecret = QStringLiteral("s3cret+&=");
    QVERIFY(client.requestClientCredentials(s));
    QVERIFY(client.isBusy());
    QVERIFY(!client.requestClientCredentials(s));

    const std::optional<OAuthResult> result = waitFor(client);
    QVERIFY2(result->ok, qPrintable(result->errorString));
    QCOMPARE(result->token.accessToken, QStringLiteral("cc-token"));
    QVERIFY(result->token.isUsable());
    QVERIFY(!client.isBusy());
}

void OAuthClientTest::authorizationCodeWithPkce()
{
    OAuthClient client;
    QSignalSpy back(&client, &OAuthClient::callbackReceived);
    QString error;
    const QUrl url = client.startAuthorization(settings(), &error);
    QVERIFY2(url.isValid(), qPrintable(error));
    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue(QStringLiteral("prompt")), QStringLiteral("login"));
    QCOMPARE(query.queryItemValue(QStringLiteral("response_type")), QStringLiteral("code"));
    QCOMPARE(query.queryItemValue(QStringLiteral("code_challenge_method")), QStringLiteral("S256"));
    const QUrl redirect(query.queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded));
    QCOMPARE(redirect.host(), QStringLiteral("127.0.0.1"));
    const QString state = query.queryItemValue(QStringLiteral("state"));
    const QByteArray challenge = query.queryItemValue(QStringLiteral("code_challenge")).toLatin1();

    // The browser comes back.
    QNetworkAccessManager browser;
    QUrl callback = redirect;
    callback.setQuery(QStringLiteral("code=abc&state=") + state);
    QNetworkReply *page = browser.get(QNetworkRequest(callback));
    const std::optional<OAuthResult> result = waitFor(client);
    QTRY_VERIFY(page->isFinished());
    QVERIFY(page->readAll().contains("logged in"));
    page->deleteLater();

    QCOMPARE(back.count(), 1);
    QVERIFY2(result->ok, qPrintable(result->errorString));
    QCOMPARE(result->token.accessToken, QStringLiteral("ac-token"));
    QCOMPARE(result->token.refreshToken, QStringLiteral("r1"));
    // The mock echoes the verifier it received as the scope.
    QCOMPARE(OAuthClient::codeChallenge(result->token.scope.toLatin1()), challenge);
}

void OAuthClientTest::callbackChecksState()
{
    OAuthClient client;
    const QUrl url = client.startAuthorization(settings());
    QVERIFY(url.isValid());
    QUrl callback(QUrlQuery(url).queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded));
    callback.setQuery(QStringLiteral("code=abc&state=forged"));
    QNetworkAccessManager browser;
    QNetworkReply *page = browser.get(QNetworkRequest(callback));
    QTRY_VERIFY(page->isFinished());
    QVERIFY(page->readAll().contains("not the login"));
    page->deleteLater();
    QVERIFY(client.isBusy());

    client.cancel();
    QVERIFY(!client.isBusy());
    QVERIFY(!client.startAuthorization(OAuth2Settings()).isValid());
}

void OAuthClientTest::errorsAreReported()
{
    OAuthClient client;
    const QUrl url = client.startAuthorization(settings());
    QUrl callback(QUrlQuery(url).queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded));
    callback.setQuery(QStringLiteral("error=access_denied&error_description=No%20thanks&state=")
                      + QUrlQuery(url).queryItemValue(QStringLiteral("state")));
    QNetworkAccessManager browser;
    QNetworkReply *page = browser.get(QNetworkRequest(callback));
    std::optional<OAuthResult> result = waitFor(client);
    page->deleteLater();
    QVERIFY(!result->ok);
    QVERIFY2(result->errorString.contains(QStringLiteral("No thanks")), qPrintable(result->errorString));

    OAuthClient other;
    OAuth2Settings s = settings();
    s.grant = OAuthGrant::ClientCredentials;
    s.clientSecret = QStringLiteral("wrong");
    QVERIFY(other.requestClientCredentials(s));
    result = waitFor(other);
    QVERIFY(!result->ok);
    QVERIFY2(result->errorString.contains(QStringLiteral("The grant is not valid.")), qPrintable(result->errorString));
}

void OAuthClientTest::refreshAndExpiry()
{
    OAuthClient client;
    QVERIFY(client.refresh(settings(), QStringLiteral("r1")));
    const std::optional<OAuthResult> result = waitFor(client);
    QVERIFY2(result->ok, qPrintable(result->errorString));
    QCOMPARE(result->token.accessToken, QStringLiteral("refreshed"));
    QVERIFY(result->token.expiresAt > QDateTime::currentDateTimeUtc().addSecs(100));

    OAuthToken token;
    token.accessToken = QStringLiteral("t");
    QVERIFY(token.isUsable());
    token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(20);
    QVERIFY(!token.isUsable());
    token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(60);
    QVERIFY(token.isUsable());
    QVERIFY(!client.refresh(settings(), QString()));
}

QTEST_GUILESS_MAIN(OAuthClientTest)

#include "tst_oauthclient.moc"
