// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mockhttpserver.h"

#include <QFile>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
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
    case 503:
        return QByteArrayLiteral("Service Unavailable");
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
                   const QByteArray &body, bool headOnly = false, bool withReason = true)
{
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' '
                          + (withReason ? reasonPhrase(status) : QByteArray()) + "\r\n";
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
}

/*!
 * \brief Starts listening on an ephemeral port of the loopback interface.
 * \return True when the server listens.
 */
bool MockHttpServer::start()
{
    m_server = std::make_unique<QTcpServer>();
    connect(m_server.get(), &QTcpServer::pendingConnectionAvailable, this,
            &MockHttpServer::acceptConnections);
    return m_server->listen(QHostAddress::LocalHost, 0);
}

/*!
 * \brief Starts listening for HTTPS on an ephemeral port of the loopback interface.
 *
 * The certificate and key in testcert/ are for tests only. Connections become
 * pending after the handshake, so the routes see decrypted requests.
 * \return True when the files could be read and the server listens.
 */
bool MockHttpServer::startTls()
{
    QFile certificateFile(QStringLiteral(REQDECK_TEST_CERT_DIR "/test-only.crt"));
    QFile keyFile(QStringLiteral(REQDECK_TEST_CERT_DIR "/test-only.key"));
    if (!certificateFile.open(QIODevice::ReadOnly) || !keyFile.open(QIODevice::ReadOnly))
        return false;
    const QSslCertificate certificate(&certificateFile, QSsl::Pem);
    const QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem);
    if (certificate.isNull() || key.isNull())
        return false;

    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    configuration.setLocalCertificate(certificate);
    configuration.setPrivateKey(key);
    configuration.setPeerVerifyMode(QSslSocket::VerifyNone);

    auto server = std::make_unique<QSslServer>();
    server->setSslConfiguration(configuration);
    m_server = std::move(server);
    m_tls = true;
    connect(m_server.get(), &QTcpServer::pendingConnectionAvailable, this,
            &MockHttpServer::acceptConnections);
    return m_server->listen(QHostAddress::LocalHost, 0);
}

/*!
 * \brief Returns the listening port, or 0 before start().
 */
quint16 MockHttpServer::port() const
{
    return m_server ? m_server->serverPort() : quint16(0);
}

/*!
 * \brief Returns the absolute URL of \a pathAndQuery on this server.
 * \param pathAndQuery Path with optional query, starting with '/'.
 */
QUrl MockHttpServer::url(const QString &pathAndQuery) const
{
    return url(pathAndQuery, QStringLiteral("127.0.0.1"));
}

/*!
 * \brief Returns the absolute URL of \a pathAndQuery on this server under \a host.
 * \param pathAndQuery Path with optional query, starting with '/'.
 * \param host A name of the loopback interface, such as localhost.
 */
QUrl MockHttpServer::url(const QString &pathAndQuery, const QString &host) const
{
    return QUrl(QStringLiteral("%1://%2:%3%4")
                    .arg(m_tls ? QStringLiteral("https") : QStringLiteral("http"), host)
                    .arg(port())
                    .arg(pathAndQuery));
}

/*!
 * \brief Returns the number of complete requests received so far.
 */
int MockHttpServer::requestCount() const
{
    return m_requestCount;
}

/*!
 * \brief Returns \a size bytes of the text /bytes/ serves, starting at \a offset.
 *
 * Lines of 63 letters and a line feed, each line shifted by one letter, so that
 * a byte out of place shows in a comparison.
 * \param offset Position of the first byte.
 * \param size Number of bytes.
 */
QByteArray MockHttpServer::patternBytes(qint64 offset, qint64 size)
{
    QByteArray bytes(size, Qt::Uninitialized);
    for (qint64 i = 0; i < size; ++i) {
        const qint64 position = offset + i;
        bytes[i] = position % 64 == 63 ? '\n' : char('a' + (position / 64 + position % 64) % 26);
    }
    return bytes;
}

/*!
 * \brief Accepts pending connections and wires their input and cleanup.
 */
void MockHttpServer::acceptConnections()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
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

    if (path.startsWith("/bytes/")) {
        // Written as the socket drains, so the server never holds the body.
        const qint64 total = path.mid(7).toLongLong();
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + QByteArray::number(total)
                      + "\r\nConnection: close\r\n\r\n");
        if (head || total == 0) {
            socket->disconnectFromHost();
            return;
        }
        auto sent = std::make_shared<qint64>(0);
        const auto writeMore = [socket, total, sent]() {
            constexpr qint64 kChunk = 256 * 1024;
            while (*sent < total && socket->bytesToWrite() < 4 * kChunk) {
                const qint64 size = qMin(kChunk, total - *sent);
                socket->write(patternBytes(*sent, size));
                *sent += size;
                if (*sent == total)
                    socket->disconnectFromHost();
            }
        };
        connect(socket, &QTcpSocket::bytesWritten, socket, writeMore);
        writeMore();
        return;
    }

    if (path.startsWith("/no-reason/")) {
        // The status line of an HTTP/2 answer as Qt reports it: no reason phrase.
        writeResponse(socket, path.mid(11).toInt(), {{"Content-Type", "text/plain"}}, "body", head, false);
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

    if (path == "/markup") {
        // A page would load this image; an HTTP client must only show the text.
        const QByteArray pixel = "<img src=\"http://127.0.0.1:" + QByteArray::number(port())
                                 + "/pixel.png\">";
        writeResponse(socket, 200, {{"Content-Type", "text/plain"}, {"X-Markup", pixel}}, "ok", head);
        return;
    }

    if (path.startsWith("/long/")) {
        writeResponse(socket, 200, {{"Content-Type", "text/plain"}},
                      QByteArray(path.mid(6).toInt(), 'x'), head);
        return;
    }

    if (path == "/crlf") {
        // Windows line ends: the text view keeps one separator for each.
        writeResponse(socket, 200, {{"Content-Type", "text/plain"}},
                      "first\r\nneedle one\r\nneedle two\r\n", head);
        return;
    }

    if (path.startsWith("/lines/")) {
        QByteArray body = "[0";
        const int rows = path.mid(7).toInt();
        for (int row = 1; row < rows; ++row)
            body += ",0";
        writeResponse(socket, 200, {{"Content-Type", "application/json"}}, body + "]", head);
        return;
    }

    if (path.startsWith("/json/")) {
        QByteArray body = "[";
        const int rows = path.mid(6).toInt();
        for (int row = 0; row < rows; ++row) {
            if (row > 0)
                body += ',';
            body += "{\"id\":" + QByteArray::number(row) + ",\"name\":\"row\"}";
        }
        writeResponse(socket, 200, {{"Content-Type", "application/json"}}, body + "]", head);
        return;
    }

    if (path == "/binary") {
        writeResponse(socket, 200, {{"Content-Type", "application/octet-stream"}},
                      QByteArray::fromHex("89504E4700010203"), head);
        return;
    }

    if (path == "/response-headers") {
        writeResponse(socket, 200, {{"X-Dup", "a"}, {"X-Dup", "b"}, {"Content-Type", "text/plain"}},
                      "ok", head);
        return;
    }

    writeResponse(socket, 404, {{"Content-Type", "text/plain"}}, "not found", head);
}
