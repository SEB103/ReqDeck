// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RESPONSEDATA_H
#define RESPONSEDATA_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

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

/** Result of one executed request. */
struct ResponseData
{
    /** HTTP status code; 0 when no response was received. */
    int statusCode {0};
    /** HTTP reason phrase, for example "Not Found". */
    QString reasonPhrase;
    /** Response headers in received order; Qt lower-cases names and merges repeated ones. */
    QList<std::pair<QByteArray, QByteArray>> headers;
    /** Response body after transparent decompression. */
    QByteArray body;
    /** Value of the Content-Type header; empty when absent. */
    QString contentType;
    /** Time from sending to completion in milliseconds. */
    qint64 elapsedMs {0};
    /** Body size in bytes. */
    qint64 sizeBytes {0};
    /** URL of the final response after redirects. */
    QUrl finalUrl;
    /** Transport failure, or ResponseError::None. */
    ResponseError error {ResponseError::None};
    /** Human-readable failure text; empty without an error. */
    QString errorString;
    /** TLS validation messages reported for the connection. */
    QStringList tlsErrors;

    /** Returns whether an HTTP response with a status code was received. */
    bool hasStatus() const { return statusCode > 0; }
};

#endif // RESPONSEDATA_H
