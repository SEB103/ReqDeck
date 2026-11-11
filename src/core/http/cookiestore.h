// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COOKIESTORE_H
#define COOKIESTORE_H

#include <QByteArray>
#include <QList>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QString>
#include <QUrl>

#include "core/workspace/secretcipher.h"

/**
 * Keeps the cookies of one workspace.
 *
 * Matching, expiry, and the checks on Set-Cookie follow QNetworkCookieJar.
 * The store is never set on a QNetworkAccessManager: HttpExecutor adds and
 * stores cookies itself on every hop. Persistent cookies can be saved to a
 * file encrypted for the Windows user; session cookies are never written.
 */
class CookieStore : public QNetworkCookieJar
{
    Q_OBJECT

public:
    /** Most cookies one store keeps; the oldest-expiring go first beyond it. */
    static constexpr int kMaxCookies = 3000;

    /** Creates an empty store. */
    explicit CookieStore(QObject *parent = nullptr);

    /** Returns every cookie that has not expired, ordered by domain, path, and name. */
    QList<QNetworkCookie> cookies() const;

    /** Stores the cookies of the Set-Cookie \a values of a response from \a url; returns how many were taken. */
    int storeFromResponse(const QList<QByteArray> &values, const QUrl &url);

    /** Stores \a cookies a response from \a url set; returns how many were taken. */
    int storeCookies(const QList<QNetworkCookie> &cookies, const QUrl &url);

    /** Returns the Cookie header value for a request to \a url, or an empty array. */
    QByteArray cookieHeader(const QUrl &url) const;

    /** Adds \a cookie or replaces the one with its name, domain, and path; false for an invalid cookie. */
    bool setCookie(const QNetworkCookie &cookie);

    /** Removes the cookie with the name, domain, and path of \a cookie; false when there is none. */
    bool removeCookie(const QNetworkCookie &cookie);

    /** Removes every cookie. */
    void clear();

    /** Replaces the cookies with the persistent ones saved at \a path; a missing file means none. */
    bool load(const QString &path, const SecretCipher &cipher, QString *errorString = nullptr);

    /** Saves the persistent cookies to \a path encrypted; without any the file is removed. */
    bool save(const QString &path, const SecretCipher &cipher, QString *errorString = nullptr) const;

signals:
    /** Emitted after the cookies changed. */
    void changed();

private:
    /** Drops expired cookies and, beyond kMaxCookies, those that expire first. */
    void trim();
};

#endif // COOKIESTORE_H
