// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OAUTHCLIENT_H
#define OAUTHCLIENT_H

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "core/http/requestspec.h"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QTcpServer;
QT_END_NAMESPACE

/** A token from an OAuth 2.0 authorization server. */
struct OAuthToken
{
    /** Access token sent as Bearer. */
    QString accessToken;
    /** Refresh token, or empty when the server gave none. */
    QString refreshToken;
    /** Token type, normally "Bearer". */
    QString tokenType;
    /** Scopes the server granted, separated by spaces. */
    QString scope;
    /** When the token expires; invalid when the server did not say. */
    QDateTime expiresAt;

    /** Returns whether the token can be sent at \a now: present and not within 30 s of its expiry. */
    bool isUsable(const QDateTime &now = QDateTime::currentDateTimeUtc()) const;

    bool operator==(const OAuthToken &) const = default;
};

/** The outcome of getting a token. */
struct OAuthResult
{
    /** Whether a token came. */
    bool ok {false};
    /** Why not, for the user. */
    QString errorString;
    /** The token. */
    OAuthToken token;
};

/**
 * Gets OAuth 2.0 tokens: Authorization Code with PKCE (S256) through the
 * system browser and a one-shot loopback callback on 127.0.0.1, Client
 * Credentials, and refresh. One flow at a time; each ends in one finished().
 */
class OAuthClient : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(OAuthClient)

public:
    /** Milliseconds the loopback callback waits for the browser. */
    static constexpr int kCallbackTimeoutMs = 5 * 60 * 1000;

    /** Creates an idle client. */
    explicit OAuthClient(QObject *parent = nullptr);

    /** Stops a running flow without finished(). */
    ~OAuthClient() override;

    /** Returns whether a flow is running. */
    bool isBusy() const;

    /** Returns a new PKCE code verifier: 64 characters of the unreserved set. */
    static QByteArray codeVerifier();

    /** Returns the S256 code challenge of \a verifier. */
    static QByteArray codeChallenge(const QByteArray &verifier);

    /** Opens the callback and returns the URL to open in the browser, or an empty URL with \a errorString. */
    QUrl startAuthorization(const OAuth2Settings &settings, QString *errorString = nullptr);

    /** Gets a token with the client's own credentials. */
    bool requestClientCredentials(const OAuth2Settings &settings);

    /** Gets a new token with \a refreshToken. */
    bool refresh(const OAuth2Settings &settings, const QString &refreshToken);

    /** Cancels the running flow, which finishes with an error. */
    void cancel();

signals:
    /** Emitted once per started flow with its outcome. */
    void finished(const OAuthResult &result);

    /** Emitted when the browser came back to the callback, before the code is exchanged. */
    void callbackReceived();

private:
    /** Answers the browser's request on the callback and exchanges the code. */
    void readCallback();

    /** Posts \a form to the token endpoint of \a settings. */
    bool postToken(const OAuth2Settings &settings, const QList<std::pair<QString, QString>> &form);

    /** Turns the token endpoint's reply into a result and finishes. */
    void readToken();

    /** Stops the callback listener and the timer. */
    void stopListening();

    /** Ends the flow with \a result. */
    void finish(const OAuthResult &result);

    /** Network manager for token requests; owned through the QObject tree. */
    QNetworkAccessManager *m_network {nullptr};

    /** Running token request, or null. */
    QPointer<QNetworkReply> m_reply;

    /** Whether the running token request was stopped for an answer over the size limit. */
    bool m_answerTooLarge {false};

    /** Loopback listener of the authorization code flow, or null. */
    QTcpServer *m_server {nullptr};

    /** Ends the wait for the browser. */
    QTimer m_timeout;

    /** Settings of the running flow. */
    OAuth2Settings m_settings;

    /** PKCE verifier of the running authorization. */
    QByteArray m_verifier;

    /** State the callback must carry back. */
    QByteArray m_state;

    /** Redirect URI of the running authorization. */
    QString m_redirectUri;

    /** Whether a flow is running. */
    bool m_busy {false};
};

#endif // OAUTHCLIENT_H
