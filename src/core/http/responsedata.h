// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RESPONSEDATA_H
#define RESPONSEDATA_H

#include <QByteArray>
#include <QList>
#include <QTemporaryFile>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtNetwork/qtnetworkglobal.h>
#if QT_CONFIG(ssl)
#include <QSslError>
#endif

#include <algorithm>
#include <memory>
#include <utility>

/** Transport-level failure of a request; HTTP error statuses are not failures. */
enum class ResponseError {
    /** The server answered (any status code). */
    None,
    /** The transfer timeout expired. */
    Timeout,
    /** The user cancelled the request. */
    Aborted,
    /** The server refused the connection. */
    ConnectionRefused,
    /** The host name could not be resolved. */
    HostNotFound,
    /** The TLS handshake or certificate validation failed. */
    Tls,
    /** Any other network failure. */
    Network
};

/** Certificate errors of one server a request reached, and whether they were ignored. */
struct TlsCheck
{
    /** Host name as in the URL; empty for no check. */
    QString host;
    /** Port of the connection. */
    int port {-1};
    /** Messages of the errors, without repeats. */
    QStringList messages;
#if QT_CONFIG(ssl)
    /** The errors, each with the certificate it concerns. */
    QList<QSslError> errors;
#endif
    /** Whether the errors were ignored, so that this server was not authenticated. */
    bool ignored {false};

    /** Returns whether this is no check at all. */
    bool isEmpty() const { return host.isEmpty(); }

    /** Returns the host, followed by the port unless it is the HTTPS default. */
    QString server() const
    {
        return port < 0 || port == 443 ? host : host + u':' + QString::number(port);
    }

    /**
     * Returns whether \a other is this server with only errors this check also had,
     * each for the same certificate, so that accepting this check accepts \a other.
     */
    bool covers(const TlsCheck &other) const
    {
        if (host.isEmpty() || host.compare(other.host, Qt::CaseInsensitive) != 0 || port != other.port)
            return false;
#if QT_CONFIG(ssl)
        return !other.errors.isEmpty()
               && std::all_of(other.errors.cbegin(), other.errors.cend(),
                              [this](const QSslError &error) { return errors.contains(error); });
#else
        return false;
#endif
    }
};

/** Result of one executed request. */
struct ResponseData
{
    /** HTTP status code; 0 when no response was received. */
    int statusCode {0};
    /** HTTP reason phrase, for example "Not Found". */
    QString reasonPhrase;
    /** Response headers in received order; Qt lower-cases names and merges repeated ones. */
    QList<std::pair<QByteArray, QByteArray>> headers;
    /** Response body after transparent decompression; only its start when bodyFile is set. */
    QByteArray body;
    /** The whole body when it was too large for memory; deleted with the last copy. */
    std::shared_ptr<QTemporaryFile> bodyFile;
    /** Value of the Content-Type header; empty when absent. */
    QString contentType;
    /** Time from sending to completion in milliseconds. */
    qint64 elapsedMs {0};
    /** Body size in bytes, the whole body also when only its start is in memory. */
    qint64 sizeBytes {0};
    /** URL of the final response after redirects. */
    QUrl finalUrl;
    /** Transport failure, or ResponseError::None. */
    ResponseError error {ResponseError::None};
    /** Human-readable failure text; empty without an error. */
    QString errorString;
    /** Servers whose certificate had errors, in the order the request reached them. */
    QList<TlsCheck> tlsChecks;

    /** Returns whether an HTTP response with a status code was received. */
    bool hasStatus() const { return statusCode > 0; }

    /** Returns whether certificate errors of a server were ignored, so it was not authenticated. */
    bool tlsErrorsIgnored() const
    {
        return std::any_of(tlsChecks.cbegin(), tlsChecks.cend(), [](const TlsCheck &check) { return check.ignored; });
    }
};

#endif // RESPONSEDATA_H
