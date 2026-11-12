// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "core/http/oauthtokenstore.h"

/*! Verifies the store of OAuth 2.0 tokens. */
class OAuthTokenStoreTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that a changed setting gives another key, while the secret is no part of it. */
    void changedSettingsNeedNewToken();

    /*! Verifies keeping, replacing, and resetting tokens by node, and the kept refresh token. */
    void keepsAndResets();

    /*! Verifies that tokens survive a save and load encrypted, and a foreign file is refused. */
    void savesEncrypted();

private:
    /*! A token that expires in an hour. */
    static OAuthToken makeToken(const QString &access, const QString &refresh);
};

OAuthToken OAuthTokenStoreTest::makeToken(const QString &access, const QString &refresh)
{
    OAuthToken token;
    token.accessToken = access;
    token.refreshToken = refresh;
    token.tokenType = QStringLiteral("Bearer");
    token.expiresAt = QDateTime::currentDateTimeUtc().addSecs(3600);
    return token;
}

void OAuthTokenStoreTest::changedSettingsNeedNewToken()
{
    OAuth2Settings settings;
    settings.tokenUrl = QStringLiteral("https://id.test/token");
    settings.clientId = QStringLiteral("app");
    settings.clientSecret = QStringLiteral("top-secret");
    const QString key = OAuthTokenStore::keyFor(QStringLiteral("r1"), settings);
    QVERIFY(key.startsWith(QStringLiteral("r1/")));
    QVERIFY(!key.contains(QStringLiteral("top-secret")));
    QCOMPARE(OAuthTokenStore::keyFor(QStringLiteral("r1"), settings), key);

    for (auto change : {+[](OAuth2Settings &s) { s.clientId = QStringLiteral("other"); },
                        +[](OAuth2Settings &s) { s.scope = QStringLiteral("admin"); },
                        +[](OAuth2Settings &s) { s.tokenUrl = QStringLiteral("https://evil.test/token"); },
                        +[](OAuth2Settings &s) { s.grant = OAuthGrant::ClientCredentials; }}) {
        OAuth2Settings changed = settings;
        change(changed);
        QVERIFY(OAuthTokenStore::keyFor(QStringLiteral("r1"), changed) != key);
    }
    QVERIFY(OAuthTokenStore::keyFor(QStringLiteral("r2"), settings) != key);
    OAuth2Settings rotated = settings;
    rotated.clientSecret = QStringLiteral("rotated");
    QCOMPARE(OAuthTokenStore::keyFor(QStringLiteral("r1"), rotated), key);
}

void OAuthTokenStoreTest::keepsAndResets()
{
    OAuthTokenStore store;
    QSignalSpy changed(&store, &OAuthTokenStore::changed);
    store.setToken(QStringLiteral("r1/a"), makeToken(QStringLiteral("t1"), QStringLiteral("refresh-1")));
    store.setToken(QStringLiteral("r1/a"), makeToken(QStringLiteral("t2"), QString()));
    store.setToken(QStringLiteral("r10/a"), makeToken(QStringLiteral("other"), QString()));

    QCOMPARE(store.token(QStringLiteral("r1/a"))->accessToken, QStringLiteral("t2"));
    QCOMPARE(store.token(QStringLiteral("r1/a"))->refreshToken, QStringLiteral("refresh-1"));
    QVERIFY(store.hasNode(QStringLiteral("r1")));
    QVERIFY(!store.token(QStringLiteral("r1/b")));

    QVERIFY(store.removeNode(QStringLiteral("r1")));
    QVERIFY(!store.hasNode(QStringLiteral("r1")));
    QVERIFY(store.hasNode(QStringLiteral("r10")));
    QVERIFY(!store.removeNode(QStringLiteral("r1")));
    QCOMPARE(changed.count(), 4);
}

void OAuthTokenStoreTest::savesEncrypted()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("w.tokens.json"));
    OAuthTokenStore store;
    store.setToken(QStringLiteral("r1/a"), makeToken(QStringLiteral("access-secret"), QStringLiteral("refresh-secret")));
    QVERIFY(store.save(path, SecretCipher()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    QVERIFY(!content.contains("access-secret"));
    QVERIFY(!content.contains("refresh-secret"));
    file.close();

    OAuthTokenStore loaded;
    QVERIFY(loaded.load(path, SecretCipher()));
    QCOMPARE(loaded.token(QStringLiteral("r1/a"))->accessToken, QStringLiteral("access-secret"));
    QVERIFY(loaded.token(QStringLiteral("r1/a"))->isUsable());

    QString error;
    QVERIFY(!loaded.load(path, SecretCipher("another computer"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!loaded.hasNode(QStringLiteral("r1")));

    store.clear();
    QVERIFY(store.save(path, SecretCipher()));
    QVERIFY(!QFile::exists(path));
}

QTEST_GUILESS_MAIN(OAuthTokenStoreTest)

#include "tst_oauthtokenstore.moc"
