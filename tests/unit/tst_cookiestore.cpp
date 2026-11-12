// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDateTime>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "core/http/cookiestore.h"

/*! Verifies how the cookie store keeps, matches, and saves cookies. */
class CookieStoreTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that a host-only cookie goes back to its host and nowhere else. */
    void cookiesStayWithTheirHost();

    /*! Verifies that a domain cookie also goes to subdomains, and a cookie for another domain is refused. */
    void domainCookiesAndRefusals();

    /*! Verifies path matching, longer paths first, and Secure only over HTTPS. */
    void pathAndSecure();

    /*! Verifies that an expired Set-Cookie removes the stored cookie. */
    void expiryRemoves();

    /*! Verifies adding, replacing, and removing cookies by hand, with the signal. */
    void editsByHand();

    /*! Verifies that persistent cookies survive a save and load, and session cookies do not. */
    void savesPersistentOnly();

    /*! Verifies that a file of another user or a damaged file empties the store with an error. */
    void loadRefusesForeignFiles();

    /*! Verifies that the store keeps at most kMaxCookies, dropping those that expire first. */
    void limitsTheCount();

    /*! Verifies that plain HTTP neither sets a Secure cookie nor replaces one. */
    void plainHttpLeavesSecureCookiesAlone();
};

void CookieStoreTest::cookiesStayWithTheirHost()
{
    CookieStore store;
    QCOMPARE(store.storeFromResponse({"session=abc; Path=/; HttpOnly"}, QUrl(QStringLiteral("https://a.test/login"))), 1);

    QCOMPARE(store.cookieHeader(QUrl(QStringLiteral("https://a.test/orders"))), QByteArray("session=abc"));
    QCOMPARE(store.cookieHeader(QUrl(QStringLiteral("https://b.test/orders"))), QByteArray());
    QCOMPARE(store.cookieHeader(QUrl(QStringLiteral("https://sub.a.test/orders"))), QByteArray());
}

void CookieStoreTest::domainCookiesAndRefusals()
{
    CookieStore store;
    QCOMPARE(store.storeFromResponse({"wide=1; Domain=shop.test; Path=/"}, QUrl(QStringLiteral("https://api.shop.test/"))), 1);
    QCOMPARE(store.storeFromResponse({"evil=1; Domain=other.test"}, QUrl(QStringLiteral("https://api.shop.test/"))), 0);
    QCOMPARE(store.storeFromResponse({"tld=1; Domain=com"}, QUrl(QStringLiteral("https://shop.com/"))), 0);

    QCOMPARE(store.cookieHeader(QUrl(QStringLiteral("https://www.shop.test/"))), QByteArray("wide=1"));
    QCOMPARE(store.cookieHeader(QUrl(QStringLiteral("https://other.test/"))), QByteArray());
}

void CookieStoreTest::pathAndSecure()
{
    CookieStore store;
    const QUrl url(QStringLiteral("https://a.test/api/v1/x"));
    store.storeFromResponse({"root=1; Path=/", "api=2; Path=/api", "safe=3; Path=/; Secure"}, url);

    const QByteArray header = store.cookieHeader(QUrl(QStringLiteral("https://a.test/api/v1/y")));
    QVERIFY2(header.startsWith("api=2; "), header);
    QVERIFY(header.contains("root=1"));
    QVERIFY(header.contains("safe=3"));
    QCOMPARE(store.cookieHeader(QUrl(QStringLiteral("https://a.test/other"))).contains("api=2"), false);
    QCOMPARE(store.cookieHeader(QUrl(QStringLiteral("http://a.test/api"))).contains("safe=3"), false);
}

void CookieStoreTest::expiryRemoves()
{
    CookieStore store;
    const QUrl url(QStringLiteral("https://a.test/"));
    store.storeFromResponse({"token=1; Max-Age=3600"}, url);
    QCOMPARE(store.cookies().size(), 1);

    QSignalSpy changed(&store, &CookieStore::changed);
    QCOMPARE(store.storeFromResponse({"token=; Expires=Thu, 01 Jan 1970 00:00:00 GMT"}, url), 1);

    QCOMPARE(changed.count(), 1);
    QCOMPARE(store.cookies().size(), 0);
    QCOMPARE(store.cookieHeader(url), QByteArray());
}

void CookieStoreTest::editsByHand()
{
    CookieStore store;
    QSignalSpy changed(&store, &CookieStore::changed);
    QNetworkCookie cookie("id", "1");
    cookie.setDomain(QStringLiteral("a.test"));
    QVERIFY(store.setCookie(cookie));
    QCOMPARE(store.cookies().at(0).path(), QStringLiteral("/"));

    cookie.setValue("2");
    QVERIFY(store.setCookie(cookie));
    QCOMPARE(store.cookies().size(), 1);
    QCOMPARE(store.cookies().at(0).value(), QByteArray("2"));
    QVERIFY(!store.setCookie(QNetworkCookie("noDomain", "x")));

    QVERIFY(store.removeCookie(store.cookies().at(0)));
    QVERIFY(!store.removeCookie(cookie));
    QVERIFY(store.cookies().isEmpty());
    QCOMPARE(changed.count(), 3);
}

void CookieStoreTest::savesPersistentOnly()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("w.cookies.json"));
    const SecretCipher cipher;
    CookieStore store;
    store.storeFromResponse({"keep=1; Max-Age=86400; Path=/", "session=2"}, QUrl(QStringLiteral("https://a.test/")));

    QVERIFY(store.save(path, cipher));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(!file.readAll().contains("keep"));
    file.close();

    CookieStore loaded;
    QVERIFY(loaded.load(path, cipher));
    QCOMPARE(loaded.cookies().size(), 1);
    QCOMPARE(loaded.cookies().at(0).name(), QByteArray("keep"));
    QCOMPARE(loaded.cookieHeader(QUrl(QStringLiteral("https://a.test/"))), QByteArray("keep=1"));

    loaded.clear();
    QVERIFY(loaded.save(path, cipher));
    QVERIFY(!QFile::exists(path));
}

void CookieStoreTest::loadRefusesForeignFiles()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("w.cookies.json"));
    CookieStore store;
    store.storeFromResponse({"keep=1; Max-Age=86400"}, QUrl(QStringLiteral("https://a.test/")));
    QVERIFY(store.save(path, SecretCipher()));

    QString error;
    QVERIFY(!store.load(path, SecretCipher("another computer"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(store.cookies().isEmpty());

    QFile damaged(path);
    QVERIFY(damaged.open(QIODevice::WriteOnly));
    damaged.write("{oops");
    damaged.close();
    QVERIFY(!store.load(path, SecretCipher(), &error));
    QVERIFY(store.load(dir.filePath(QStringLiteral("missing.json")), SecretCipher()));
}

void CookieStoreTest::limitsTheCount()
{
    CookieStore store;
    const QUrl url(QStringLiteral("https://a.test/"));
    QList<QByteArray> values;
    for (int i = 0; i < CookieStore::kMaxCookies + 10; ++i)
        values.append("c" + QByteArray::number(i) + "=1; Max-Age=" + QByteArray::number(1000 + i));
    store.storeFromResponse(values, url);

    const QList<QNetworkCookie> kept = store.cookies();
    QCOMPARE(kept.size(), CookieStore::kMaxCookies);
    const bool earliestDropped = std::none_of(kept.cbegin(), kept.cend(), [](const QNetworkCookie &cookie) {
        return cookie.name() == "c0";
    });
    QVERIFY(earliestDropped);
}

void CookieStoreTest::plainHttpLeavesSecureCookiesAlone()
{
    CookieStore store;
    QCOMPARE(store.storeFromResponse({"flag=1; Secure"}, QUrl(QStringLiteral("http://a.test/"))), 0);
    QVERIFY(store.cookies().isEmpty());

    QCOMPARE(store.storeFromResponse({"sid=good; Secure; Path=/"}, QUrl(QStringLiteral("https://a.test/"))), 1);
    QCOMPARE(store.storeFromResponse({"sid=evil; Path=/"}, QUrl(QStringLiteral("http://a.test/"))), 0);
    QCOMPARE(store.cookies().size(), 1);
    QCOMPARE(store.cookies().at(0).value(), QByteArray("good"));
    QCOMPARE(store.storeFromResponse({"other=1; Path=/"}, QUrl(QStringLiteral("http://a.test/"))), 1);
}

QTEST_GUILESS_MAIN(CookieStoreTest)

#include "tst_cookiestore.moc"
