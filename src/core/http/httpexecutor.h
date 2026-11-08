// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HTTPEXECUTOR_H
#define HTTPEXECUTOR_H

#include <QElapsedTimer>
#include <QNetworkReply>
#include <QObject>
#include <QStringList>

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
 * never stored.
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

    /** TLS messages reported for the running request. */
    QStringList m_tlsErrors;

    /** Whether abort() was called for the running request. */
    bool m_abortRequested {false};

    /** Whether certificate errors were ignored for the running request. */
    bool m_tlsErrorsIgnored {false};

    /** Whether TLS certificate errors are ignored. */
    bool m_ignoreTlsErrors {false};
};

#endif // HTTPEXECUTOR_H
