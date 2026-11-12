// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/oauthclient.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

#include "core/http/httpexecutor.h"

#include <chrono>

namespace {
/*!
 * \internal
 * \brief Characters a PKCE verifier is made of (RFC 7636 section 4.1).
 */
constexpr char kUnreserved[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

/*!
 * \internal
 * \brief Milliseconds a token request may take.
 */
constexpr int kTokenTimeoutMs = 30000;

/*!
 * \internal
 * \brief Returns a failed result with \a message.
 */
OAuthResult failure(const QString &message)
{
    OAuthResult result;
    result.errorString = message;
    return result;
}

/*!
 * \internal
 * \brief Returns the page the browser shows after the callback, with \a message.
 */
QByteArray callbackPage(const QString &message)
{
    const QByteArray body = "<!doctype html><meta charset=\"utf-8\"><title>ReqDeck</title>"
                            "<body style=\"font-family:sans-serif;margin:3em\"><p>"
                            + message.toHtmlEscaped().toUtf8() + "</p></body>";
    return "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
           + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}
} // namespace

/*!
 * \brief Returns whether the token can be sent at \a now.
 *
 * A token without an expiry counts until the server refuses it; one with an
 * expiry stops 30 s early, so it does not run out on the way.
 */
bool OAuthToken::isUsable(const QDateTime &now) const
{
    return !accessToken.isEmpty() && (!expiresAt.isValid() || now.addSecs(30) < expiresAt);
}

/*!
 * \class OAuthClient
 * \inmodule ReqDeckCore
 * \brief Gets OAuth 2.0 tokens.
 */

/*!
 * \brief Creates an idle client owned by \a parent.
 */
OAuthClient::OAuthClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setCookieJar(nullptr);
    HttpExecutor::useSystemProxy(m_network);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(kCallbackTimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        stopListening();
        finish(failure(tr("The login in the browser did not come back within 5 minutes.")));
    });
}

/*!
 * \brief Stops a running flow without emitting finished().
 */
OAuthClient::~OAuthClient()
{
    m_busy = false;
    stopListening();
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
}

/*!
 * \brief Returns whether a flow is running.
 */
bool OAuthClient::isBusy() const
{
    return m_busy;
}

/*!
 * \brief Returns a new PKCE code verifier of 64 unreserved characters.
 */
QByteArray OAuthClient::codeVerifier()
{
    QByteArray verifier(64, Qt::Uninitialized);
    QRandomGenerator *random = QRandomGenerator::system();
    for (char &c : verifier)
        c = kUnreserved[random->bounded(int(sizeof(kUnreserved) - 1))];
    return verifier;
}

/*!
 * \brief Returns the S256 code challenge of \a verifier: base64url of its SHA-256, without padding.
 */
QByteArray OAuthClient::codeChallenge(const QByteArray &verifier)
{
    return QCryptographicHash::hash(verifier, QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

/*!
 * \brief Starts the authorization code flow.
 *
 * Listens on 127.0.0.1 with a port the system picks (RFC 8252 section 7.3) and
 * returns the authorization URL with the redirect URI, a random state, and
 * the S256 challenge. The caller opens it in the browser. The flow finishes
 * when the browser comes back with the right state, after the code exchange,
 * or after kCallbackTimeoutMs.
 * \param settings The settings with variables already resolved.
 * \param errorString Receives why the flow could not start; may be null.
 * \return The URL to open, or an empty URL when busy or the settings are incomplete.
 */
QUrl OAuthClient::startAuthorization(const OAuth2Settings &settings, QString *errorString)
{
    const auto fail = [errorString](const QString &message) {
        if (errorString)
            *errorString = message;
        return QUrl();
    };
    if (m_busy)
        return fail(tr("A login is already running."));
    const QUrl authorization(settings.authUrl.trimmed());
    if (!authorization.isValid() || authorization.host().isEmpty()
        || !QStringList {QStringLiteral("http"), QStringLiteral("https")}.contains(authorization.scheme()))
        return fail(tr("The authorization URL is not valid."));
    if (settings.clientId.trimmed().isEmpty())
        return fail(tr("The client id is missing."));

    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        const QString error = m_server->errorString();
        stopListening();
        return fail(tr("The login could not wait for the browser: %1").arg(error));
    }
    connect(m_server, &QTcpServer::newConnection, this, &OAuthClient::readCallback);

    m_settings = settings;
    m_verifier = codeVerifier();
    m_state = codeVerifier().left(32);
    m_redirectUri = QStringLiteral("http://127.0.0.1:%1/callback").arg(m_server->serverPort());

    // Encoded as in postToken(), so '+' and '%' reach the server as typed.
    const auto encoded = [](const QString &value) { return QString::fromLatin1(QUrl::toPercentEncoding(value)); };
    QUrlQuery query(authorization);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("client_id"), encoded(settings.clientId.trimmed()));
    query.addQueryItem(QStringLiteral("redirect_uri"), encoded(m_redirectUri));
    if (!settings.scope.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("scope"), encoded(settings.scope.trimmed()));
    if (!settings.audience.trimmed().isEmpty())
        query.addQueryItem(QStringLiteral("audience"), encoded(settings.audience.trimmed()));
    query.addQueryItem(QStringLiteral("state"), QString::fromLatin1(m_state));
    query.addQueryItem(QStringLiteral("code_challenge"), QString::fromLatin1(codeChallenge(m_verifier)));
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    QUrl url = authorization;
    url.setQuery(query);

    m_busy = true;
    m_timeout.start();
    return url;
}

/*!
 * \brief Gets a token with the client's own id and secret.
 * \param settings The settings with variables resolved.
 * \return False when busy or the token URL is not valid; finished() follows otherwise.
 */
bool OAuthClient::requestClientCredentials(const OAuth2Settings &settings)
{
    if (m_busy)
        return false;
    QList<std::pair<QString, QString>> form {{QStringLiteral("grant_type"), QStringLiteral("client_credentials")}};
    if (!settings.scope.trimmed().isEmpty())
        form.append({QStringLiteral("scope"), settings.scope.trimmed()});
    if (!settings.audience.trimmed().isEmpty())
        form.append({QStringLiteral("audience"), settings.audience.trimmed()});
    m_settings = settings;
    return postToken(settings, form);
}

/*!
 * \brief Gets a new token with \a refreshToken.
 * \param settings The settings with variables resolved.
 * \param refreshToken The refresh token the server gave.
 * \return False when busy or the token URL is not valid; finished() follows otherwise.
 */
bool OAuthClient::refresh(const OAuth2Settings &settings, const QString &refreshToken)
{
    if (m_busy || refreshToken.isEmpty())
        return false;
    QList<std::pair<QString, QString>> form {{QStringLiteral("grant_type"), QStringLiteral("refresh_token")},
                                              {QStringLiteral("refresh_token"), refreshToken}};
    if (!settings.scope.trimmed().isEmpty())
        form.append({QStringLiteral("scope"), settings.scope.trimmed()});
    m_settings = settings;
    return postToken(settings, form);
}

/*!
 * \brief Cancels the running flow; it finishes with an error.
 */
void OAuthClient::cancel()
{
    if (!m_busy)
        return;
    stopListening();
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
    finish(failure(tr("The login was cancelled.")));
}

/*!
 * \internal
 * \brief Answers the browser's request on the callback and exchanges the code.
 *
 * Only "GET /callback" with the state of this flow counts; anything else gets
 * a short page and is ignored, so another program cannot end the flow.
 */
void OAuthClient::readCallback()
{
    while (QTcpSocket *socket = m_server ? m_server->nextPendingConnection() : nullptr) {
        // Kept by the client: the server goes away after the callback, the answer must not.
        socket->setParent(this);
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            if (!socket->canReadLine() && socket->bytesAvailable() < 8192)
                return;
            const QList<QByteArray> requestLine = socket->readLine(8192).trimmed().split(' ');
            const QUrl target = requestLine.size() >= 2 ? QUrl(QString::fromLatin1(requestLine.at(1))) : QUrl();
            const QUrlQuery query(target);
            socket->disconnect(this);
            const bool ours = requestLine.value(0) == "GET" && target.path() == QLatin1StringView("/callback")
                              && query.queryItemValue(QStringLiteral("state")).toLatin1() == m_state && !m_state.isEmpty();
            if (!ours) {
                socket->write(callbackPage(tr("This is not the login ReqDeck waits for.")));
                socket->disconnectFromHost();
                return;
            }
            const QString error = query.queryItemValue(QStringLiteral("error"), QUrl::FullyDecoded);
            const QString code = query.queryItemValue(QStringLiteral("code"), QUrl::FullyDecoded);
            socket->write(callbackPage(error.isEmpty() ? tr("You are logged in. You can close this window and go back to ReqDeck.")
                                                       : tr("The login did not work. You can close this window.")));
            socket->disconnectFromHost();
            // Consumed: a second callback with the same state is not ours any more.
            m_state.clear();
            stopListening();
            emit callbackReceived();
            if (!error.isEmpty() || code.isEmpty()) {
                const QString description = query.queryItemValue(QStringLiteral("error_description"), QUrl::FullyDecoded);
                finish(failure(tr("The login was refused: %1").arg(description.isEmpty() ? error : description)));
                return;
            }
            QList<std::pair<QString, QString>> form {{QStringLiteral("grant_type"), QStringLiteral("authorization_code")},
                                                      {QStringLiteral("code"), code},
                                                      {QStringLiteral("redirect_uri"), m_redirectUri},
                                                      {QStringLiteral("code_verifier"), QString::fromLatin1(m_verifier)}};
            m_busy = false;
            if (!postToken(m_settings, form))
                finish(failure(tr("The token URL is not valid.")));
        });
    }
}

/*!
 * \internal
 * \brief Posts \a form to the token endpoint, with the client id and, if set, the secret.
 *
 * The client authenticates in the body (client_secret_post), which most
 * servers accept; RFC 6749 prefers HTTP Basic, which some servers require.
 * \param settings The settings with variables resolved.
 * \param form The grant's own fields.
 */
bool OAuthClient::postToken(const OAuth2Settings &settings, const QList<std::pair<QString, QString>> &form)
{
    const QUrl url(settings.tokenUrl.trimmed());
    if (!url.isValid() || url.host().isEmpty()
        || !QStringList {QStringLiteral("http"), QStringLiteral("https")}.contains(url.scheme()))
        return false;
    QUrlQuery body;
    for (const auto &[key, value] : form)
        body.addQueryItem(key, QString::fromLatin1(QUrl::toPercentEncoding(value)));
    body.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(QUrl::toPercentEncoding(settings.clientId.trimmed())));
    if (!settings.clientSecret.isEmpty())
        body.addQueryItem(QStringLiteral("client_secret"), QString::fromLatin1(QUrl::toPercentEncoding(settings.clientSecret)));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(std::chrono::milliseconds(kTokenTimeoutMs));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    m_busy = true;
    m_reply = m_network->post(request, body.toString(QUrl::FullyEncoded).toLatin1());
    connect(m_reply, &QNetworkReply::finished, this, &OAuthClient::readToken);
    return true;
}

/*!
 * \internal
 * \brief Turns the token endpoint's reply into a result.
 *
 * A JSON object with access_token is a token; an "error" member, as RFC 6749
 * section 5.2 defines it, or anything else is a failure with the server's words.
 */
void OAuthClient::readToken()
{
    QNetworkReply *reply = m_reply;
    m_reply.clear();
    if (!reply)
        return;
    reply->deleteLater();
    const QByteArray data = reply->readAll();
    const QJsonObject object = QJsonDocument::fromJson(data).object();
    const QString access = object.value(QLatin1StringView("access_token")).toString();
    if (!access.isEmpty()) {
        OAuthResult result;
        result.ok = true;
        result.token.accessToken = access;
        result.token.refreshToken = object.value(QLatin1StringView("refresh_token")).toString();
        result.token.tokenType = object.value(QLatin1StringView("token_type")).toString(QStringLiteral("Bearer"));
        result.token.scope = object.value(QLatin1StringView("scope")).toString(m_settings.scope.trimmed());
        const qint64 expiresIn = object.value(QLatin1StringView("expires_in")).toVariant().toLongLong();
        if (expiresIn > 0)
            result.token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
        finish(result);
        return;
    }
    const QString error = object.value(QLatin1StringView("error")).toString();
    const QString description = object.value(QLatin1StringView("error_description")).toString();
    if (!error.isEmpty())
        finish(failure(tr("The authorization server refused the token: %1").arg(description.isEmpty() ? error : description)));
    else if (reply->error() != QNetworkReply::NoError)
        finish(failure(tr("The token could not be fetched: %1").arg(reply->errorString())));
    else
        finish(failure(tr("The authorization server answered without a token.")));
}

/*!
 * \internal
 * \brief Stops the callback listener and its timer.
 */
void OAuthClient::stopListening()
{
    m_timeout.stop();
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

/*!
 * \internal
 * \brief Ends the flow with \a result and forgets its secrets.
 */
void OAuthClient::finish(const OAuthResult &result)
{
    m_busy = false;
    m_verifier.clear();
    m_state.clear();
    m_settings = OAuth2Settings();
    emit finished(result);
}
