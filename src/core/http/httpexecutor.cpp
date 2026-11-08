// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/httpexecutor.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QHttpHeaders>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#if QT_CONFIG(ssl)
#include <QSslError>
#endif

#include <utility>

namespace {
/*!
 * \internal
 * \brief Maximum number of redirects followed for one request.
 */
constexpr int kMaxRedirects = 10;

/*!
 * \internal
 * \brief Returns whether \a status is a redirect that carries a Location to follow.
 */
bool isFollowableRedirect(int status)
{
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

/*!
 * \internal
 * \brief Returns the standard reason phrase of \a status, or an empty string.
 *
 * HTTP/2 and HTTP/3 carry no reason phrase, so a status would otherwise show as
 * a bare number. The phrases are those of RFC 9110 and the common extensions.
 */
QString standardReasonPhrase(int status)
{
    switch (status) {
    case 100: return QStringLiteral("Continue");
    case 101: return QStringLiteral("Switching Protocols");
    case 200: return QStringLiteral("OK");
    case 201: return QStringLiteral("Created");
    case 202: return QStringLiteral("Accepted");
    case 203: return QStringLiteral("Non-Authoritative Information");
    case 204: return QStringLiteral("No Content");
    case 205: return QStringLiteral("Reset Content");
    case 206: return QStringLiteral("Partial Content");
    case 207: return QStringLiteral("Multi-Status");
    case 300: return QStringLiteral("Multiple Choices");
    case 301: return QStringLiteral("Moved Permanently");
    case 302: return QStringLiteral("Found");
    case 303: return QStringLiteral("See Other");
    case 304: return QStringLiteral("Not Modified");
    case 307: return QStringLiteral("Temporary Redirect");
    case 308: return QStringLiteral("Permanent Redirect");
    case 400: return QStringLiteral("Bad Request");
    case 401: return QStringLiteral("Unauthorized");
    case 402: return QStringLiteral("Payment Required");
    case 403: return QStringLiteral("Forbidden");
    case 404: return QStringLiteral("Not Found");
    case 405: return QStringLiteral("Method Not Allowed");
    case 406: return QStringLiteral("Not Acceptable");
    case 407: return QStringLiteral("Proxy Authentication Required");
    case 408: return QStringLiteral("Request Timeout");
    case 409: return QStringLiteral("Conflict");
    case 410: return QStringLiteral("Gone");
    case 411: return QStringLiteral("Length Required");
    case 412: return QStringLiteral("Precondition Failed");
    case 413: return QStringLiteral("Content Too Large");
    case 414: return QStringLiteral("URI Too Long");
    case 415: return QStringLiteral("Unsupported Media Type");
    case 416: return QStringLiteral("Range Not Satisfiable");
    case 417: return QStringLiteral("Expectation Failed");
    case 418: return QStringLiteral("I'm a teapot");
    case 421: return QStringLiteral("Misdirected Request");
    case 422: return QStringLiteral("Unprocessable Content");
    case 423: return QStringLiteral("Locked");
    case 425: return QStringLiteral("Too Early");
    case 426: return QStringLiteral("Upgrade Required");
    case 428: return QStringLiteral("Precondition Required");
    case 429: return QStringLiteral("Too Many Requests");
    case 431: return QStringLiteral("Request Header Fields Too Large");
    case 451: return QStringLiteral("Unavailable For Legal Reasons");
    case 500: return QStringLiteral("Internal Server Error");
    case 501: return QStringLiteral("Not Implemented");
    case 502: return QStringLiteral("Bad Gateway");
    case 503: return QStringLiteral("Service Unavailable");
    case 504: return QStringLiteral("Gateway Timeout");
    case 505: return QStringLiteral("HTTP Version Not Supported");
    case 511: return QStringLiteral("Network Authentication Required");
    default: return {};
    }
}

/*!
 * \internal
 * \brief Asks the system for the proxy of each request, except for this computer.
 *
 * Windows keeps the proxy of browsers and of most applications in one place;
 * a company network often needs it. A local server, such as one under
 * development, must never go through it, whatever the bypass list says. The
 * factory holds no state, so it is safe on the threads Qt asks it from.
 */
class SystemProxyFactory final : public QNetworkProxyFactory
{
public:
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery &query) override
    {
        if (HttpExecutor::isThisComputer(query.peerHostName()))
            return {QNetworkProxy(QNetworkProxy::NoProxy)};
        return QNetworkProxyFactory::systemProxyForQuery(query);
    }
};

/*!
 * \internal
 * \brief Returns whether \a lhs and \a rhs share scheme, host, and effective port.
 */
bool isSameOrigin(const QUrl &lhs, const QUrl &rhs)
{
    const auto defaultPort = [](const QUrl &url) {
        return url.port(url.scheme().compare(QLatin1StringView("https"), Qt::CaseInsensitive) == 0
                            ? 443
                            : 80);
    };
    return lhs.scheme().compare(rhs.scheme(), Qt::CaseInsensitive) == 0
           && lhs.host().compare(rhs.host(), Qt::CaseInsensitive) == 0
           && defaultPort(lhs) == defaultPort(rhs);
}
} // namespace

/*!
 * \brief Creates an idle executor with its own network manager.
 *
 * The manager gets no cookie jar: Qt would otherwise store Set-Cookie values
 * and send them with later requests, replacing Cookie headers the user typed.
 * It asks the system for a proxy (see SystemProxyFactory).
 * \param parent The QObject parent.
 */
HttpExecutor::HttpExecutor(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setCookieJar(nullptr);
    m_network->setProxyFactory(new SystemProxyFactory);
}

qint64 HttpExecutor::bodyMemoryLimit() const
{
    return m_bodyMemoryLimit;
}

/*!
 * \brief Sets the memory limit of a body.
 * \param bytes The limit; tests use a small one to reach the file without huge bodies.
 */
void HttpExecutor::setBodyMemoryLimit(qint64 bytes)
{
    m_bodyMemoryLimit = bytes;
}

/*!
 * \brief Returns the proxies a request to \a url would try.
 * \param url The address of a request.
 */
QList<QNetworkProxy> HttpExecutor::proxiesFor(const QUrl &url) const
{
    return m_network->proxyFactory()->queryProxy(QNetworkProxyQuery(url));
}

/*!
 * \brief Removes the temporary files of large bodies that an earlier run left behind.
 *
 * A body file is deleted with its response, but not when the application
 * crashed or was killed. Only files untouched for a day go: a younger one may
 * belong to another running instance, which would lose the body it can still
 * save, because Windows lets a file be deleted while it is open.
 * \param directory The temporary folder, normally QDir::tempPath().
 * \return The number of files removed.
 */
int HttpExecutor::removeStaleBodyFiles(const QString &directory)
{
    int removed = 0;
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);
    const QDir folder(directory);
    const QFileInfoList files = folder.entryInfoList({QStringLiteral("ReqDeck-*.body")}, QDir::Files);
    for (const QFileInfo &file : files) {
        if (file.lastModified() < cutoff && QFile::remove(file.filePath()))
            ++removed;
    }
    return removed;
}

/*!
 * \brief Returns whether \a host is this computer.
 *
 * Besides loopback addresses, localhost and every name under it count, with or
 * without the final dot of a fully qualified name: RFC 6761 reserves them for
 * this computer, and browsers resolve them so.
 * \param host A host name or address, as in a URL.
 */
bool HttpExecutor::isThisComputer(const QString &host)
{
    if (QHostAddress(host).isLoopback())
        return true;
    QStringView name(host);
    if (name.endsWith(u'.'))
        name.chop(1);
    return name.compare(QLatin1StringView("localhost"), Qt::CaseInsensitive) == 0
           || name.endsWith(QLatin1StringView(".localhost"), Qt::CaseInsensitive);
}

/*!
 * \brief Cancels a running request without emitting finished() or busyChanged().
 *
 * The reply is disconnected before it is aborted, because QNetworkReply::abort()
 * emits QNetworkReply::finished synchronously and the handler must not run on
 * a half-destroyed executor.
 */
HttpExecutor::~HttpExecutor()
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        delete m_reply;
    }
}

/*!
 * \brief Returns whether a request is in flight.
 */
bool HttpExecutor::isBusy() const
{
    return m_reply != nullptr;
}

/*!
 * \brief Returns whether TLS certificate errors are ignored.
 */
bool HttpExecutor::ignoreTlsErrors() const
{
    return m_ignoreTlsErrors;
}

/*!
 * \brief Sets whether TLS certificate errors are ignored.
 *
 * Applies to requests started afterwards. Ignoring errors disables server
 * authentication, so the UI must make this state visible.
 * \param ignore True to accept invalid certificates.
 */
void HttpExecutor::setIgnoreTlsErrors(bool ignore)
{
    m_ignoreTlsErrors = ignore;
}

/*!
 * \brief Starts \a request asynchronously.
 *
 * Redirects are followed by the executor according to
 * BuiltRequest::followRedirects (see handleFinished()). Certificate errors
 * are ignored only where BuiltRequest::tlsException covers them: the same host
 * and port, and no error it did not have, so a redirect to another server or a
 * changed certificate still fails. Do not delete the
 * executor from a slot connected to busyChanged() or finished(); use
 * deleteLater() instead.
 * \param request A request produced by RequestBuilder.
 * \return False without side effects when a request is already running or
 *         \a request is not ok; true when the request was started.
 */
bool HttpExecutor::send(const BuiltRequest &request)
{
    if (m_reply || !request.ok)
        return false;

    m_current = request;
    m_redirectCount = 0;
    m_abortRequested = false;
    m_tlsChecks.clear();
    m_timer.start();
    startReply();

    emit busyChanged(true);
    return true;
}

/*!
 * \brief Cancels the running request.
 *
 * The reply finishes synchronously inside QNetworkReply::abort(), so
 * finished() with ResponseError::Aborted is emitted before this returns.
 */
void HttpExecutor::abort()
{
    if (!m_reply)
        return;
    m_abortRequested = true;
    m_reply->abort();
}

/*!
 * \brief Maps a Qt network error to a ResponseError.
 *
 * Qt reports both a user abort and an expired transfer timeout as
 * OperationCanceledError; \a abortRequested tells them apart.
 * \param error The error reported by QNetworkReply.
 * \param abortRequested Whether abort() was called for the request.
 */
ResponseError HttpExecutor::classifyError(QNetworkReply::NetworkError error, bool abortRequested)
{
    switch (error) {
    case QNetworkReply::NoError:
        return ResponseError::None;
    case QNetworkReply::OperationCanceledError:
        return abortRequested ? ResponseError::Aborted : ResponseError::Timeout;
    case QNetworkReply::TimeoutError:
        return ResponseError::Timeout;
    case QNetworkReply::ConnectionRefusedError:
        return ResponseError::ConnectionRefused;
    case QNetworkReply::HostNotFoundError:
        return ResponseError::HostNotFound;
    case QNetworkReply::SslHandshakeFailedError:
        return ResponseError::Tls;
    default:
        return ResponseError::Network;
    }
}

/*!
 * \brief Sends the current hop of m_current and wires the reply.
 *
 * HEAD uses QNetworkAccessManager::head(), which never reads a body; every
 * other verb goes through sendCustomRequest() so that bodies work for all of
 * them.
 */
void HttpExecutor::startReply()
{
    if (m_current.verb == "HEAD")
        m_reply = m_network->head(m_current.request);
    else
        m_reply = m_network->sendCustomRequest(m_current.request, m_current.verb, m_current.body);

    m_body.clear();
    m_bodyFile.reset();
    m_bodySize = 0;
    m_bodyError.clear();
    connect(m_reply, &QNetworkReply::readyRead, this, &HttpExecutor::readBody);
    connect(m_reply, &QNetworkReply::finished, this, &HttpExecutor::handleFinished);
#if QT_CONFIG(ssl)
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::sslErrors, this, [this, reply](const QList<QSslError> &errors) {
        // The check is kept also when it is ignored: the response must say
        // which server was not authenticated, and why.
        TlsCheck check;
        check.host = reply->url().host();
        check.port = reply->url().port(443);
        check.errors = errors;
        for (const QSslError &error : errors) {
            if (!check.messages.contains(error.errorString()))
                check.messages.append(error.errorString());
        }
        check.ignored = m_ignoreTlsErrors || m_current.tlsException.covers(check);
        if (check.ignored)
            reply->ignoreSslErrors();
        m_tlsChecks.append(std::move(check));
    });
#endif
}

/*!
 * \brief Moves the bytes that arrived to memory, or to the temporary file.
 *
 * Reading as the data arrives keeps Qt from buffering the whole body. Once the
 * body passes the memory limit, everything so far goes to a temporary file and
 * so does the rest, while the start stays in memory for display. A failed write,
 * for example on a full disk, stops the transfer; handleFinished() reports it.
 */
void HttpExecutor::readBody()
{
    if (!m_reply || !m_reply->isOpen() || !m_bodyError.isEmpty())
        return;
    const QByteArray data = m_reply->readAll();
    m_bodySize += data.size();

    if (!m_bodyFile) {
        m_body += data;
        if (m_body.size() <= m_bodyMemoryLimit)
            return;
        auto file = std::make_shared<QTemporaryFile>(QDir::temp().filePath(QStringLiteral("ReqDeck-XXXXXX.body")));
        if (!file->open() || file->write(m_body) != m_body.size()) {
            m_bodyError = file->errorString();
            m_reply->abort();
            return;
        }
        m_bodyFile = std::move(file);
        m_body.truncate(qMin(m_bodyMemoryLimit, kBodyPreviewBytes));
        m_body.squeeze();
        return;
    }

    if (m_bodyFile->write(data) != data.size()) {
        m_bodyError = m_bodyFile->errorString();
        m_reply->abort();
    }
}

/*!
 * \brief Prepares m_current for following a redirect to \a target with \a status.
 *
 * Mirrors what browsers and curl do: 303, and 301/302 after POST, continue as
 * GET without body; 307/308 repeat method and body. When the target is on
 * another origin, every header in BuiltRequest::credentialHeaders is removed,
 * so credentials are not handed to a third party.
 */
void HttpExecutor::prepareRedirect(const QUrl &target, int status)
{
    QNetworkRequest next = m_current.request;
    QHttpHeaders headers = next.headers();

    if (!isSameOrigin(next.url(), target)) {
        for (const QByteArray &name : std::as_const(m_current.credentialHeaders))
            headers.removeAll(QLatin1StringView(name));
    }

    const bool toGet = m_current.verb != "HEAD"
                       && (status == 303 || ((status == 301 || status == 302) && m_current.verb == "POST"));
    if (toGet) {
        m_current.verb = QByteArrayLiteral("GET");
        m_current.body.clear();
        headers.removeAll(QHttpHeaders::WellKnownHeader::ContentType);
        headers.removeAll(QHttpHeaders::WellKnownHeader::ContentLength);
    }

    next.setUrl(target);
    next.setHeaders(std::move(headers));
    m_current.request = next;
}

/*!
 * \brief Converts the finished reply into ResponseData, follows a redirect, or emits finished().
 *
 * A reply that carries an HTTP status code is a response, even for 4xx/5xx
 * statuses or an unfollowed redirect, for which Qt also sets an error code.
 * Transport failures are the exception: an abort, a timeout, or a connection
 * that broke after the status line (for example a body shorter than its
 * Content-Length) is reported as an error, with the partial response kept.
 *
 * When redirects are followed, a 301/302/303/307/308 with a target is sent
 * again (see prepareRedirect()) without emitting anything, up to
 * kMaxRedirects hops. A redirect from https to http, or to a non-HTTP scheme,
 * is not followed and returned as the response.
 *
 * busyChanged(false) is emitted before finished() so that a slot connected to
 * finished() can send the next request.
 */
void HttpExecutor::handleFinished()
{
    QNetworkReply *reply = std::exchange(m_reply, nullptr);
    reply->deleteLater();

    ResponseData response;
    response.elapsedMs = m_timer.elapsed();
    response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.reasonPhrase = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString().trimmed();
    if (response.reasonPhrase.isEmpty())
        response.reasonPhrase = standardReasonPhrase(response.statusCode);
    response.headers = reply->rawHeaderPairs();
    response.contentType = QString::fromLatin1(
        reply->headers().value(QHttpHeaders::WellKnownHeader::ContentType));
    // An aborted or timed-out reply is already closed; reading it only warns.
    m_reply = reply;
    readBody();
    m_reply = nullptr;
    if (m_bodyFile)
        m_bodyFile->flush();
    response.body = std::exchange(m_body, {});
    response.bodyFile = std::exchange(m_bodyFile, {});
    response.sizeBytes = std::exchange(m_bodySize, 0);
    response.finalUrl = reply->url();

    const QNetworkReply::NetworkError error = reply->error();
    // Codes below ContentAccessDenied are connection and proxy failures; the
    // codes above are derived from the HTTP status itself.
    const bool transportFailure = error != QNetworkReply::NoError
                                  && (error < QNetworkReply::ContentAccessDenied
                                      || error == QNetworkReply::ProtocolFailure);
    if (m_abortRequested || transportFailure || !response.hasStatus())
        response.error = classifyError(error, m_abortRequested);
    if (!m_bodyError.isEmpty()) {
        response.error = ResponseError::Network;
        response.errorString = QStringLiteral("The response could not be stored on disk: %1").arg(m_bodyError);
        // Nothing of the body is held, so nothing of it may look saveable.
        response.body.clear();
        response.bodyFile.reset();
        response.sizeBytes = 0;
    }

    if (response.error == ResponseError::None && m_current.followRedirects
        && isFollowableRedirect(response.statusCode)) {
        const QUrl location = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const QUrl target = reply->url().resolved(location);
        const QString scheme = target.scheme().toLower();
        const bool downgrade = reply->url().scheme().compare(QLatin1StringView("https"), Qt::CaseInsensitive) == 0
                               && scheme == QLatin1StringView("http");
        const bool followable = location.isValid() && !location.isEmpty() && target.isValid()
                                && (scheme == QLatin1StringView("http") || scheme == QLatin1StringView("https"))
                                && !downgrade;
        if (followable) {
            if (m_redirectCount >= kMaxRedirects) {
                response.error = ResponseError::Network;
                response.errorString = QStringLiteral("Too many redirects.");
            } else {
                ++m_redirectCount;
                prepareRedirect(target, response.statusCode);
                startReply();
                return;
            }
        }
    }

    if (response.error != ResponseError::None && response.errorString.isEmpty())
        response.errorString = reply->errorString();
    response.tlsChecks = std::exchange(m_tlsChecks, {});
    // An accepted certificate must not outlive its request: a connection kept
    // alive would let the next request through without any check.
    if (response.tlsErrorsIgnored())
        m_network->clearConnectionCache();

    emit busyChanged(false);
    emit finished(response);
}
