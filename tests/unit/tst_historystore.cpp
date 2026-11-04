// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "core/history/historystore.h"

/*!
 * \internal
 * \brief Returns an entry for a GET of \a url.
 */
static HistoryEntry makeEntry(const QString &url)
{
    HistoryEntry entry;
    entry.request.url = url;
    entry.statusCode = 200;
    entry.elapsedMs = 42;
    return entry;
}

/*!
 * \internal
 * \brief Returns the content of the file at \a path.
 */
static QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/*!
 * \internal
 * \brief Writes \a content to \a path, replacing any existing file.
 */
static bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

/*! Verifies the persistent request history. */
class HistoryStoreTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that new entries appear first and get an id and a timestamp. */
    void addPrependsNewestFirst();

    /*! Verifies the limit of 200 entries. */
    void capsAtMaxEntries();

    /*! Verifies that entries survive a new store instance. */
    void persistsAcrossInstances();

    /*! Verifies that variables are stored as references. */
    void snapshotKeepsVariablesUnresolved();

    /*! Verifies that literal credential headers are cleared. */
    void redactsLiteralCredentialHeaders_data();
    /*! Runs one credential header case. */
    void redactsLiteralCredentialHeaders();

    /*! Verifies that literal authentication fields are cleared. */
    void redactsAuthFields();

    /*! Verifies that the header named by an API key is cleared. */
    void redactsApiKeyHeader();

    /*! Verifies that variable references stay usable. */
    void keepsVariableReferences();

    /*! Verifies that ordinary headers and the user name stay. */
    void keepsOrdinaryValues();

    /*! Verifies that add() redacts what it stores. */
    void addStoresRedactedRequest();

    /*! Verifies that an oversized raw body is left out. */
    void oversizedBodyIsOmitted();

    /*! Verifies that no history file means an empty history. */
    void missingFileIsEmpty();

    /*! Verifies that an unusable file is set aside and history keeps working. */
    void corruptHistoryIsSetAside_data();
    /*! Runs one unusable file case. */
    void corruptHistoryIsSetAside();

    /*! Verifies that one invalid entry does not lose the others. */
    void invalidEntryIsSkipped();

    /*! Verifies that a write failure keeps the entry in memory. */
    void writeFailureKeepsEntryInMemory();

    /*! Verifies that clear() empties the history and removes the file. */
    void clearRemovesFile();
};

void HistoryStoreTest::addPrependsNewestFirst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store(dir.filePath(QStringLiteral("history.json")));

    QVERIFY(store.add(makeEntry(QStringLiteral("http://a"))));
    QVERIFY(store.add(makeEntry(QStringLiteral("http://b"))));

    QCOMPARE(store.entries().size(), 2);
    QCOMPARE(store.entries().at(0).request.url, QStringLiteral("http://b"));
    QVERIFY(!store.entries().at(0).id.isEmpty());
    QVERIFY(store.entries().at(0).id != store.entries().at(1).id);
    QVERIFY(store.entries().at(0).timestamp.isValid());
}

void HistoryStoreTest::capsAtMaxEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store(dir.filePath(QStringLiteral("history.json")));

    for (int i = 0; i <= HistoryStore::kMaxEntries; ++i)
        QVERIFY(store.add(makeEntry(QStringLiteral("http://h/%1").arg(i))));

    QCOMPARE(store.entries().size(), 200);
    QCOMPARE(store.entries().first().request.url, QStringLiteral("http://h/200"));
    QCOMPARE(store.entries().last().request.url, QStringLiteral("http://h/1"));
}

void HistoryStoreTest::persistsAcrossInstances()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));

    HistoryStore store(path);
    HistoryEntry failed = makeEntry(QStringLiteral("http://down"));
    failed.statusCode = 0;
    failed.errorString = QStringLiteral("Connection refused");
    failed.timestamp = QDateTime(QDate(2026, 11, 4), QTime(12, 30, 15, 123), QTimeZone::UTC);
    QVERIFY(store.add(makeEntry(QStringLiteral("http://a"))));
    QVERIFY(store.add(failed));

    HistoryStore reloaded(path);
    QString error;
    QVERIFY2(reloaded.load(&error), qPrintable(error));
    QVERIFY(reloaded.entries() == store.entries());
    QCOMPARE(reloaded.entries().first().timestamp, failed.timestamp);
}

void HistoryStoreTest::snapshotKeepsVariablesUnresolved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));

    HistoryStore store(path);
    QVERIFY(store.add(makeEntry(QStringLiteral("{{baseUrl}}/users"))));
    QVERIFY(readFile(path).contains("{{baseUrl}}/users"));
}

void HistoryStoreTest::redactsLiteralCredentialHeaders_data()
{
    QTest::addColumn<QString>("name");

    QTest::newRow("authorization") << QStringLiteral("authorization");
    QTest::newRow("proxy") << QStringLiteral("Proxy-Authorization");
    QTest::newRow("cookie") << QStringLiteral("Cookie");
    QTest::newRow("api-key") << QStringLiteral("x-api-key");
    QTest::newRow("auth-token") << QStringLiteral("X-Auth-Token");
    QTest::newRow("spaces") << QStringLiteral(" Authorization ");
}

void HistoryStoreTest::redactsLiteralCredentialHeaders()
{
    QFETCH(QString, name);

    RequestSpec spec;
    spec.headers = {{name, QStringLiteral("literal-secret"), true}};

    const RequestSpec result = HistoryStore::redacted(spec);
    QCOMPARE(result.headers.at(0).key, name);
    QVERIFY(result.headers.at(0).value.isEmpty());
}

void HistoryStoreTest::redactsAuthFields()
{
    RequestSpec spec;
    spec.auth.password = QStringLiteral("p");
    spec.auth.token = QStringLiteral("t");
    spec.auth.apiKeyValue = QStringLiteral("k");

    const RequestSpec result = HistoryStore::redacted(spec);
    QVERIFY(result.auth.password.isEmpty());
    QVERIFY(result.auth.token.isEmpty());
    QVERIFY(result.auth.apiKeyValue.isEmpty());
}

void HistoryStoreTest::redactsApiKeyHeader()
{
    RequestSpec spec;
    spec.auth.type = AuthType::ApiKey;
    spec.auth.apiKeyName = QStringLiteral("X-Custom-Key");
    spec.headers = {{QStringLiteral("x-custom-key"), QStringLiteral("literal"), true}};

    QVERIFY(HistoryStore::redacted(spec).headers.at(0).value.isEmpty());
}

void HistoryStoreTest::keepsVariableReferences()
{
    RequestSpec spec;
    spec.headers = {{QStringLiteral("Authorization"), QStringLiteral("Bearer {{token}}"), true}};
    spec.auth.password = QStringLiteral("{{password}}");
    spec.auth.token = QStringLiteral("{{token}}");
    spec.auth.apiKeyValue = QStringLiteral("{{key}}");

    QVERIFY(HistoryStore::redacted(spec) == spec);
}

void HistoryStoreTest::keepsOrdinaryValues()
{
    RequestSpec spec;
    spec.headers = {{QStringLiteral("Accept"), QStringLiteral("application/json"), true}};
    spec.auth.username = QStringLiteral("demo");

    QVERIFY(HistoryStore::redacted(spec) == spec);
}

void HistoryStoreTest::addStoresRedactedRequest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));

    HistoryEntry entry = makeEntry(QStringLiteral("http://a"));
    entry.request.auth.token = QStringLiteral("very-secret-token");

    HistoryStore store(path);
    QVERIFY(store.add(entry));
    QVERIFY(store.entries().at(0).request.auth.token.isEmpty());
    QVERIFY(!readFile(path).contains("very-secret-token"));
}

void HistoryStoreTest::oversizedBodyIsOmitted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store(dir.filePath(QStringLiteral("history.json")));

    HistoryEntry entry = makeEntry(QStringLiteral("http://a"));
    entry.request.body.mode = BodyMode::Raw;
    entry.request.body.raw = QString(HistoryStore::kMaxStoredBodyChars, u'x');
    QVERIFY(store.add(entry));
    QCOMPARE(store.entries().at(0).request.body.raw.size(), 65536);
    QVERIFY(!store.entries().at(0).bodyOmitted);

    entry.request.body.raw += u'x';
    QVERIFY(store.add(entry));
    QVERIFY(store.entries().at(0).request.body.raw.isEmpty());
    QVERIFY(store.entries().at(0).bodyOmitted);
}

void HistoryStoreTest::missingFileIsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store(dir.filePath(QStringLiteral("history.json")));

    QVERIFY(store.load());
    QVERIFY(store.entries().isEmpty());
}

void HistoryStoreTest::corruptHistoryIsSetAside_data()
{
    QTest::addColumn<QByteArray>("content");

    QTest::newRow("not-json") << QByteArray("{oops");
    QTest::newRow("array-root") << QByteArray("[]");
    QTest::newRow("future-schema") << QByteArray("{\"schema\":2,\"entries\":[]}");
    QTest::newRow("entries-object") << QByteArray("{\"schema\":1,\"entries\":{}}");
}

void HistoryStoreTest::corruptHistoryIsSetAside()
{
    QFETCH(QByteArray, content);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    QVERIFY(writeFile(path, content));

    HistoryStore store(path);
    QString error;
    QVERIFY(!store.load(&error));
    QVERIFY(!error.isEmpty());
    QVERIFY(store.entries().isEmpty());
    QCOMPARE(readFile(path + QStringLiteral(".corrupt")), content);

    QVERIFY(store.add(makeEntry(QStringLiteral("http://a"))));
    QCOMPARE(store.entries().size(), 1);
}

void HistoryStoreTest::invalidEntryIsSkipped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    QVERIFY(writeFile(path,
                      "{\"schema\":1,\"entries\":["
                      "{\"id\":\"a\",\"timestamp\":\"2026-11-04T12:00:00.000Z\","
                      "\"request\":{\"method\":\"GET\",\"url\":\"http://ok\"}},"
                      "{\"id\":\"b\",\"timestamp\":\"not a date\",\"request\":{}},"
                      "{\"id\":\"c\",\"timestamp\":\"2026-11-04T12:00:00.000Z\","
                      "\"request\":{\"method\":\"FETCH\"}},"
                      "7]}"));

    HistoryStore store(path);
    QVERIFY(store.load());
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().at(0).request.url, QStringLiteral("http://ok"));
}

void HistoryStoreTest::writeFailureKeepsEntryInMemory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString blocked = dir.filePath(QStringLiteral("blocked"));
    QVERIFY(writeFile(blocked, "file"));

    HistoryStore store(blocked + QStringLiteral("/history.json"));
    QString error;
    QVERIFY(!store.add(makeEntry(QStringLiteral("http://a")), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.entries().size(), 1);
}

void HistoryStoreTest::clearRemovesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));

    HistoryStore store(path);
    QVERIFY(store.add(makeEntry(QStringLiteral("http://a"))));
    QVERIFY(QFile::exists(path));

    QVERIFY(store.clear());
    QVERIFY(store.entries().isEmpty());
    QVERIFY(!QFile::exists(path));
}

QTEST_GUILESS_MAIN(HistoryStoreTest)

#include "tst_historystore.moc"
