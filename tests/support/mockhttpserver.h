// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MOCKHTTPSERVER_H
#define MOCKHTTPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QTcpServer>
#include <QUrl>

#include <utility>

QT_BEGIN_NAMESPACE
class QTcpSocket;
QT_END_NAMESPACE

/**
 * Minimal HTTP/1.1 server on 127.0.0.1 for network tests.
 *
 * Routes: /echo (request as JSON, header values decoded as UTF-8),
 * /status/<code>, /redirect (302 to /echo), /redirect307 (307 to /echo),
 * /redirect-loop (302 to itself), /redirect-to?url=<encoded URL> (302 to that
 * URL), /delay/<ms>, /response-headers (repeated X-Dup header), /set-cookie
 * (Set-Cookie: sid=server), /truncated (announces 100 body bytes, sends 10),
 * /binary (eight bytes with NUL bytes), /markup (an img tag as header value),
 * /long/<n> (n unbroken characters), /json/<n> (a JSON array of n objects);
 * anything else is 404. Every response carries Connection: close.
 */
class MockHttpServer : public QObject
{
    Q_OBJECT

public:
    /** Creates a stopped server. */
    explicit MockHttpServer(QObject *parent = nullptr);

    /** Starts listening on an ephemeral port of 127.0.0.1; true on success. */
    bool start();

    /** Returns the listening port; 0 before start(). */
    quint16 port() const;

    /** Returns the absolute URL of \a pathAndQuery on this server, for example "/echo?a=1". */
    QUrl url(const QString &pathAndQuery) const;

    /** Returns the number of complete requests received so far. */
    int requestCount() const;

private:
    /** One parsed request. */
    struct Request
    {
        /** Request method as sent. */
        QByteArray method;
        /** Path without query. */
        QByteArray path;
        /** Raw query without '?'. */
        QByteArray query;
        /** Headers in received order. */
        QList<std::pair<QByteArray, QByteArray>> headers;
        /** Request body. */
        QByteArray body;
    };

    /** Accepts pending connections. */
    void acceptConnections();

    /** Parses buffered data of \a socket and answers a complete request. */
    void readRequest(QTcpSocket *socket);

    /** Dispatches \a request to its route. */
    void route(QTcpSocket *socket, const Request &request);

    /** Listening socket. */
    QTcpServer m_server;

    /** Unparsed input per connection. */
    QHash<QTcpSocket *, QByteArray> m_buffers;

    /** Number of complete requests received. */
    int m_requestCount {0};
};

#endif // MOCKHTTPSERVER_H
