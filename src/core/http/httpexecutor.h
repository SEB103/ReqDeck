// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HTTPEXECUTOR_H
#define HTTPEXECUTOR_H

#include <QElapsedTimer>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QObject>
#include <QStringList>
#include <QTemporaryFile>

#include <memory>

#include "core/http/requestbuilder.h"
#include "core/http/responsedata.h"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
QT_END_NAMESPACE

/**
 * Sends built requests asynchronously, one at a time.
 *
 * Lives on the thread that creates it (the GUI thread) and owns its network
 * manager. Every accepted send() ends in exactly one finished() signal, also
 * after abort() or a timeout; destroying a busy executor cancels the request
 * without emitting anything. Redirects are followed here rather than by Qt,
 * so credentials can be dropped when a redirect leaves the origin. Cookies are
 * never stored. Requests use the proxy of the system, except to this computer.
 */
class HttpExecutor : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HttpExecutor)

public:
    /** Creates an idle executor. */
    explicit HttpExecutor(QObject *parent = nullptr);

    /** Cancels a running request silently and destroys the executor. */
    ~HttpExecutor() override;

    /** Returns whether a request is in flight. */
    bool isBusy() const;

    /** Returns whether TLS certificate errors are ignored. */
    bool ignoreTlsErrors() const;

    /** Sets whether TLS certificate errors are ignored; off by default. */
    void setIgnoreTlsErrors(bool ignore);

    /** Starts \a request; returns false without effect when busy or the request is not ok. */
    bool send(const BuiltRequest &request);

    /** Cancels the running request, which finishes with ResponseError::Aborted; no-op when idle. */
    void abort();

    /** Bytes of a body kept in memory before the rest goes to a temporary file. */
    static constexpr qint64 kDefaultBodyMemoryLimit = 32 * 1024 * 1024;

    /** Bytes of a body on disk kept in memory for display, at most the memory limit. */
    static constexpr qint64 kBodyPreviewBytes = 4 * 1024 * 1024;

    /** Returns the memory limit of a body. */
    qint64 bodyMemoryLimit() const;

    /** Sets the memory limit of a body to \a bytes, for requests started afterwards. */
    void setBodyMemoryLimit(qint64 bytes);

    /** Returns the proxies a request to \a url would try, in order; NoProxy means direct. */
    QList<QNetworkProxy> proxiesFor(const QUrl &url) const;

    /** Returns whether \a host is this computer: a loopback address, localhost, or a name under it. */
    static bool isThisComputer(const QString &host);

    /** Removes body files an earlier run left in \a directory; returns how many were removed. */
    static int removeStaleBodyFiles(const QString &directory);

    /** Maps a Qt network \a error to a ResponseError; \a abortRequested marks a user cancel. */
    static ResponseError classifyError(QNetworkReply::NetworkError error, bool abortRequested);

signals:
    /** Emitted when isBusy() changes to \a busy. */
    void busyChanged(bool busy);

    /** Emitted once per accepted send() with the outcome. */
    void finished(const ResponseData &response);

private:
    /** Sends the current hop of m_current and connects the reply. */
    void startReply();

    /** Moves the bytes that arrived to memory, or to the file past the limit. */
    void readBody();

    /** Rewrites m_current to follow a redirect with \a status to \a target. */
    void prepareRedirect(const QUrl &target, int status);

    /** Converts the finished reply into ResponseData, follows a redirect, or emits finished(). */
    void handleFinished();

    /** Network manager owned through the QObject tree; it has no cookie jar. */
    QNetworkAccessManager *m_network {nullptr};

    /** Running reply, or null when idle. */
    QNetworkReply *m_reply {nullptr};

    /** Request of the current hop; rewritten when a redirect is followed. */
    BuiltRequest m_current;

    /** Number of redirects followed for the running request. */
    int m_redirectCount {0};

    /** Measures the duration of the running request. */
    QElapsedTimer m_timer;

    /** Certificate checks with errors of the running request, one per server. */
    QList<TlsCheck> m_tlsChecks;

    /** Whether abort() was called for the running request. */
    bool m_abortRequested {false};


    /** Memory limit of a body. */
    qint64 m_bodyMemoryLimit {kDefaultBodyMemoryLimit};

    /** Body of the current hop in memory: all of it, or its start once m_bodyFile is set. */
    QByteArray m_body;

    /** Whole body of the current hop once it passed the limit. */
    std::shared_ptr<QTemporaryFile> m_bodyFile;

    /** Bytes of the body of the current hop received so far. */
    qint64 m_bodySize {0};

    /** Why the body could not be stored; empty while it could. */
    QString m_bodyError;

    /** Whether TLS certificate errors are ignored. */
    bool m_ignoreTlsErrors {false};
};

#endif // HTTPEXECUTOR_H
