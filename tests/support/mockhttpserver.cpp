// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mockhttpserver.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>

#include <utility>

namespace {
/*!
 * \internal
 * \brief Returns the reason phrase for \a status.
 */
QByteArray reasonPhrase(int status)
{
    switch (status) {
    case 200:
        return QByteArrayLiteral("OK");
    case 201:
        return QByteArrayLiteral("Created");
    case 204:
        return QByteArrayLiteral("No Content");
    case 302:
        return QByteArrayLiteral("Found");
    case 304:
        return QByteArrayLiteral("Not Modified");
    case 307:
        return QByteArrayLiteral("Temporary Redirect");
    case 404:
        return QByteArrayLiteral("Not Found");
    case 500:
        return QByteArrayLiteral("Internal Server Error");
    default:
        return QByteArrayLiteral("Status");
    }
}

/*!
 * \internal
 * \brief Writes one complete response to \a socket and closes the connection.
 *
 * Content-Length always reflects \a body; with \a headOnly the body itself is
 * not written, as required for HEAD responses.
 */
void writeResponse(QTcpSocket *socket, int status,
                   const QList<std::pair<QByteArray, QByteArray>> &headers,
                   const QByteArray &body, bool headOnly = false)
{
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reasonPhrase(status)
                          + "\r\n";
    for (const auto &[name, value] : headers)
        response += name + ": " + value + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    if (!headOnly)
        response += body;
    socket->write(response);
    socket->disconnectFromHost();
}
} // namespace

/*!
 * \brief Creates a stopped server.
 * \param parent The QObject parent.
 */
MockHttpServer::MockHttpServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &MockHttpServer::acceptConnections);
}

/*!
 * \brief Starts listening on an ephemeral port of the loopback interface.
 * \return True when the server listens.
 */
bool MockHttpServer::start()
{
    return m_server.listen(QHostAddress::LocalHost, 0);
}

/*!
 * \brief Returns the listening port, or 0 before start().
 */
quint16 MockHttpServer::port() const
{
    return m_server.serverPort();
}

/*!
 * \brief Returns the absolute URL of \a pathAndQuery on this server.
 * \param pathAndQuery Path with optional query, starting with '/'.
 */
QUrl MockHttpServer::url(const QString &pathAndQuery) const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(port()).arg(pathAndQuery));
}

/*!
 * \brief Returns the number of complete requests received so far.
 */
int MockHttpServer::requestCount() const
{
    return m_requestCount;
}

/*!
 * \brief Accepts pending connections and wires their input and cleanup.
 */
void MockHttpServer::acceptConnections()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { readRequest(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

/*!
 * \brief Parses the buffered input of \a socket and routes a complete request.
 *
 * A request is complete once the header block and Content-Length body bytes
 * have arrived. Each connection serves exactly one request.
 * \param socket The client connection.
 */
void MockHttpServer::readRequest(QTcpSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();

    const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;

    const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
    const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
    if (requestLine.size() < 2) {
        writeResponse(socket, 400, {}, {});
        return;
    }

    Request request;
    request.method = requestLine.at(0);
    const QByteArray target = requestLine.at(1);
    const qsizetype queryStart = target.indexOf('?');
    request.path = queryStart < 0 ? target : target.left(queryStart);
    request.query = queryStart < 0 ? QByteArray() : target.mid(queryStart + 1);

    qsizetype contentLength = 0;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QByteArray name = line.left(colon).trimmed();
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (name.compare("Content-Length", Qt::CaseInsensitive) == 0)
            contentLength = value.toLongLong();
        request.headers.append({name, value});
    }

    const qsizetype bodyStart = headerEnd + 4;
    if (buffer.size() - bodyStart < contentLength)
        return;
    request.body = buffer.mid(bodyStart, contentLength);
    buffer.clear();

    ++m_requestCount;
    route(socket, request);
}

/*!
 * \brief Answers \a request according to its path.
 * \param socket The client connection.
 * \param request The parsed request.
 */
void MockHttpServer::route(QTcpSocket *socket, const Request &request)
{
    const bool head = request.method == "HEAD";
    const QByteArray &path = request.path;

    if (path == "/echo") {
        QJsonArray headers;
        for (const auto &[name, value] : request.headers)
            headers.append(QJsonArray {QString::fromLatin1(name), QString::fromUtf8(value)});
        const QJsonObject echo {
            {QStringLiteral("method"), QString::fromLatin1(request.method)},
            {QStringLiteral("path"), QString::fromLatin1(request.path)},
            {QStringLiteral("query"), QString::fromLatin1(request.query)},
            {QStringLiteral("headers"), headers},
            {QStringLiteral("body"), QString::fromUtf8(request.body)},
        };
        writeResponse(socket, 200, {{"Content-Type", "application/json"}},
                      QJsonDocument(echo).toJson(QJsonDocument::Compact), head);
        return;
    }

    if (path.startsWith("/status/")) {
        const int status = path.mid(8).toInt();
        const bool noBody = status == 204 || status == 304;
        writeResponse(socket, status, {{"Content-Type", "text/plain"}},
                      noBody ? QByteArray() : "status " + QByteArray::number(status), head);
        return;
    }

    if (path == "/redirect") {
        writeResponse(socket, 302, {{"Location", "/echo"}}, {}, head);
        return;
    }

    if (path == "/redirect307") {
        writeResponse(socket, 307, {{"Location", "/echo"}}, {}, head);
        return;
    }

    if (path == "/redirect-loop") {
        writeResponse(socket, 302, {{"Location", "/redirect-loop"}}, {}, head);
        return;
    }

    if (path == "/redirect-to") {
        // The target is passed percent-encoded as "url=<absolute URL>".
        const QByteArray target = QByteArray::fromPercentEncoding(request.query.mid(4));
        writeResponse(socket, 302, {{"Location", target}}, {}, head);
        return;
    }

    if (path == "/set-cookie") {
        writeResponse(socket, 200, {{"Set-Cookie", "sid=server; Path=/"}, {"Content-Type", "text/plain"}},
                      "ok", head);
        return;
    }

    if (path == "/truncated") {
        // Announces 100 bytes but sends 10 and closes the connection.
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 100\r\n"
                      "Connection: close\r\n\r\n0123456789");
        socket->disconnectFromHost();
        return;
    }

    if (path.startsWith("/delay/")) {
        // The socket is the timer's context: a client that disconnects early
        // destroys it and the pending answer is dropped.
        QTimer::singleShot(path.mid(7).toInt(), socket, [socket, head]() {
            writeResponse(socket, 200, {{"Content-Type", "text/plain"}}, "delayed", head);
        });
        return;
    }

    if (path == "/response-headers") {
        writeResponse(socket, 200, {{"X-Dup", "a"}, {"X-Dup", "b"}, {"Content-Type", "text/plain"}},
                      "ok", head);
        return;
    }

    writeResponse(socket, 404, {{"Content-Type", "text/plain"}}, "not found", head);
}
