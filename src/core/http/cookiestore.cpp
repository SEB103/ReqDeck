// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/cookiestore.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

#include "core/workspace/jsonfile.h"

namespace {
/*!
 * \internal
 * \brief Version of the cookies file.
 */
constexpr int kCookiesSchemaVersion = 1;

/*!
 * \internal
 * \brief Protection name in the cookies file: DPAPI for the current user.
 */
constexpr QLatin1StringView kDpapiProtection("dpapi");

/*!
 * \internal
 * \brief Sets \a errorString to \a message when it is not null and returns false.
 */
bool fail(QString *errorString, const QString &message)
{
    if (errorString)
        *errorString = message;
    return false;
}
} // namespace

/*!
 * \class CookieStore
 * \inmodule ReqDeckCore
 * \brief Keeps the cookies of one workspace.
 */

/*!
 * \brief Creates an empty store owned by \a parent.
 */
CookieStore::CookieStore(QObject *parent)
    : QNetworkCookieJar(parent)
{
}

/*!
 * \brief Returns every cookie that has not expired, ordered by domain, path, and name.
 */
QList<QNetworkCookie> CookieStore::cookies() const
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QList<QNetworkCookie> result;
    for (const QNetworkCookie &cookie : allCookies()) {
        if (cookie.isSessionCookie() || cookie.expirationDate().toUTC() > now)
            result.append(cookie);
    }
    std::sort(result.begin(), result.end(), [](const QNetworkCookie &a, const QNetworkCookie &b) {
        if (a.domain() != b.domain())
            return a.domain() < b.domain();
        if (a.path() != b.path())
            return a.path() < b.path();
        return a.name() < b.name();
    });
    return result;
}

/*!
 * \brief Stores the cookies of a response.
 *
 * Each value is parsed as one Set-Cookie header. QNetworkCookieJar fills a
 * missing domain and path from \a url and refuses cookies for another domain
 * or a public suffix; a cookie with a past expiry removes the stored one.
 * \param values The Set-Cookie header values in the order received.
 * \param url The URL the response came from, per hop.
 * \return How many cookies were stored or removed.
 */
int CookieStore::storeFromResponse(const QList<QByteArray> &values, const QUrl &url)
{
    QList<QNetworkCookie> parsed;
    for (const QByteArray &value : values)
        parsed += QNetworkCookie::parseCookies(value);
    return storeCookies(parsed, url);
}

/*!
 * \brief Stores \a cookies that a response from \a url set.
 *
 * Each cookie is checked on its own, so one refused cookie does not drop the
 * others; see storeFromResponse().
 * \param cookies The parsed Set-Cookie values, in the order received.
 * \param url The URL the response came from.
 * \return How many cookies were stored or removed.
 */
int CookieStore::storeCookies(const QList<QNetworkCookie> &cookies, const QUrl &url)
{
    const bool secureOrigin = url.scheme().compare(QLatin1StringView("https"), Qt::CaseInsensitive) == 0;
    int taken = 0;
    for (const QNetworkCookie &cookie : cookies) {
        if (!secureOrigin) {
            // RFC 6265bis 5.7: plain HTTP neither sets Secure cookies nor replaces them.
            if (cookie.isSecure())
                continue;
            const QList<QNetworkCookie> stored = allCookies();
            QNetworkCookie candidate = cookie;
            if (candidate.domain().isEmpty())
                candidate.setDomain(url.host());
            if (candidate.path().isEmpty())
                candidate.setPath(QStringLiteral("/"));
            const bool shadowsSecure = std::any_of(stored.cbegin(), stored.cend(), [&](const QNetworkCookie &other) {
                return other.isSecure() && other.name() == candidate.name()
                       && QString(other.domain()).remove(0, other.domain().startsWith(u'.') ? 1 : 0)
                              == QString(candidate.domain()).remove(0, candidate.domain().startsWith(u'.') ? 1 : 0);
            });
            if (shadowsSecure)
                continue;
        }
        // Qt reports a removal by an expired cookie as false; the count shows it.
        const qsizetype before = allCookies().size();
        if (setCookiesFromUrl({cookie}, url) || allCookies().size() != before)
            ++taken;
    }
    if (taken > 0) {
        trim();
        emit changed();
    }
    return taken;
}

/*!
 * \brief Returns the Cookie header value for a request to \a url.
 *
 * QNetworkCookieJar picks the cookies by domain, path, expiry, and the Secure
 * flag, longer paths first.
 * \param url The URL of the hop about to be sent.
 * \return "name=value; name2=value2", or an empty array without cookies.
 */
QByteArray CookieStore::cookieHeader(const QUrl &url) const
{
    QList<QByteArray> pairs;
    for (const QNetworkCookie &cookie : cookiesForUrl(url))
        pairs.append(cookie.toRawForm(QNetworkCookie::NameAndValueOnly));
    return pairs.join("; ");
}

/*!
 * \brief Adds \a cookie, or replaces the stored one with its name, domain, and path.
 *
 * Used by the cookie manager, so the domain must be set; no URL check applies.
 * \param cookie The cookie.
 * \return False when the name or domain is empty.
 */
bool CookieStore::setCookie(const QNetworkCookie &cookie)
{
    if (cookie.name().isEmpty() || cookie.domain().isEmpty())
        return false;
    QNetworkCookie stored = cookie;
    if (stored.path().isEmpty())
        stored.setPath(QStringLiteral("/"));
    QList<QNetworkCookie> list = allCookies();
    list.removeIf([&](const QNetworkCookie &other) { return other.hasSameIdentifier(stored); });
    list.append(stored);
    setAllCookies(list);
    trim();
    emit changed();
    return true;
}

/*!
 * \brief Removes the cookie with the name, domain, and path of \a cookie.
 * \return False when there is no such cookie.
 */
bool CookieStore::removeCookie(const QNetworkCookie &cookie)
{
    QList<QNetworkCookie> list = allCookies();
    if (list.removeIf([&](const QNetworkCookie &other) { return other.hasSameIdentifier(cookie); }) == 0)
        return false;
    setAllCookies(list);
    emit changed();
    return true;
}

/*!
 * \brief Removes every cookie.
 */
void CookieStore::clear()
{
    if (allCookies().isEmpty())
        return;
    setAllCookies({});
    emit changed();
}

/*!
 * \brief Replaces the cookies with the persistent ones saved at \a path.
 *
 * The store is emptied first, also when reading fails, so cookies of another
 * workspace never stay. A missing file is an empty store.
 * \param path The cookies file of the workspace.
 * \param cipher The cipher that encrypted it.
 * \param errorString Receives the reason of a failure; may be null.
 * \return False when the file exists but cannot be read or decrypted.
 */
bool CookieStore::load(const QString &path, const SecretCipher &cipher, QString *errorString)
{
    setAllCookies({});
    emit changed();

    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return fail(errorString, QStringLiteral("The cookies could not be read."));
    const QJsonObject root = QJsonDocument::fromJson(file.read(64 * 1024 * 1024)).object();
    if (root.value(QLatin1StringView("schema")).toInt() != kCookiesSchemaVersion
        || root.value(QLatin1StringView("protection")).toString() != kDpapiProtection)
        return fail(errorString, QStringLiteral("The cookies file is not one this version reads."));
    const std::optional<QByteArray> plain =
        cipher.unprotect(QByteArray::fromBase64(root.value(QLatin1StringView("data")).toString().toLatin1()));
    if (!plain)
        return fail(errorString, QStringLiteral("The cookies were saved for another Windows user or computer."));

    QList<QNetworkCookie> list;
    for (const QByteArray &line : plain->split('\n')) {
        for (const QNetworkCookie &cookie : QNetworkCookie::parseCookies(line)) {
            if (!cookie.name().isEmpty() && !cookie.domain().isEmpty() && !cookie.isSessionCookie())
                list.append(cookie);
        }
    }
    setAllCookies(list);
    trim();
    emit changed();
    return true;
}

/*!
 * \brief Saves the persistent cookies to \a path, encrypted for the Windows user.
 *
 * Session cookies are left out: they end with the workspace. Without any
 * persistent cookie the file is removed.
 * \param path The cookies file of the workspace.
 * \param cipher The cipher to encrypt with.
 * \param errorString Receives the reason of a failure; may be null.
 * \return True on success.
 */
bool CookieStore::save(const QString &path, const SecretCipher &cipher, QString *errorString) const
{
    QList<QByteArray> lines;
    for (const QNetworkCookie &cookie : cookies()) {
        if (!cookie.isSessionCookie())
            lines.append(cookie.toRawForm(QNetworkCookie::Full));
    }
    if (lines.isEmpty()) {
        if (QFile::exists(path) && !QFile::remove(path))
            return fail(errorString, QStringLiteral("The cookies could not be written."));
        return true;
    }
    const std::optional<QByteArray> sealed = cipher.protect(lines.join('\n'));
    if (!sealed)
        return fail(errorString, QStringLiteral("The cookies could not be encrypted."));

    QJsonObject root;
    root.insert(QLatin1StringView("schema"), kCookiesSchemaVersion);
    root.insert(QLatin1StringView("protection"), kDpapiProtection);
    root.insert(QLatin1StringView("data"), QString::fromLatin1(sealed->toBase64()));
    return writeJsonFile(path, root) || fail(errorString, QStringLiteral("The cookies could not be written."));
}

/*!
 * \internal
 * \brief Drops expired cookies and, beyond kMaxCookies, those that expire first.
 *
 * Session cookies count as expiring last, since they end with the workspace.
 */
void CookieStore::trim()
{
    QList<QNetworkCookie> list = cookies();
    if (list.size() > kMaxCookies) {
        std::stable_sort(list.begin(), list.end(), [](const QNetworkCookie &a, const QNetworkCookie &b) {
            if (a.isSessionCookie() != b.isSessionCookie())
                return b.isSessionCookie();
            return a.expirationDate() < b.expirationDate();
        });
        list.remove(0, list.size() - kMaxCookies);
    }
    setAllCookies(list);
}
