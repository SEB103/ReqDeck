// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QLockFile>
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

    /*! Verifies what is left of a URL holding credentials. */
    void redactsUrlCredentials_data();
    /*! Runs one URL case. */
    void redactsUrlCredentials();

    /*! Verifies rows whose names say they hold credentials. */
    void redactsCredentialRows();

    /*! Verifies the usual short and vendor names of credentials. */
    void redactsCommonCredentialNames_data();
    /*! Runs one name case. */
    void redactsCommonCredentialNames();

    /*! Verifies that a value shows it is a credential whatever its header is called. */
    void redactsHeaderValuesWithAuthScheme();

    /*! Verifies a token standing in the place of a user name, as in curl -u token: */
    void redactsTokenInUsername();

    /*! Verifies what is left of a raw body holding credentials. */
    void redactsRawBody_data();
    /*! Runs one raw body case. */
    void redactsRawBody();

    /*! Verifies a credential after a long value, near the size limit. */
    void redactsLongRawBody();

    /*! Verifies that ordinary rows stay as they are. */
    void keepsOrdinaryRows();

    /*! Verifies values mixing a literal part with a reference. */
    void redactsPartlyLiteralValues_data();
    /*! Runs one mixed value case. */
    void redactsPartlyLiteralValues();

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

    /*! Verifies that add() without load() keeps what the file already holds. */
    void addBeforeLoadKeepsExistingEntries();

    /*! Verifies that a file that cannot be read is never overwritten. */
    void unreadableHistoryIsNotOverwritten();

    /*! Verifies that a write failure keeps the entry in memory. */
    void writeFailureKeepsEntryInMemory();

    /*! Verifies that clear() empties the history and removes the file. */
    void clearRemovesFile();

    /*! Verifies that two stores on one file both keep their entries. */
    void twoStoresKeepBothEntries();

    /*! Verifies that a clear in one store is not undone by the other. */
    void clearAfterOtherStoreAdded();

    /*! Verifies that prepareAdd() tells a plain prepend from a merge. */
    void prepareAddReportsMerge();

    /*! Verifies that every stored text is capped, not only the raw body. */
    void longValuesAreCapped();

    /*! Verifies that clear() also removes a set-aside corrupt file. */
    void clearRemovesCorruptFile();

    /*! Verifies that a clear while another instance holds the lock changes nothing. */
    void clearWhileLockedKeepsEntries();
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

void HistoryStoreTest::redactsUrlCredentials_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expected");

    QTest::newRow("user and password") << QStringLiteral("https://alice:pw@h/x")
                                       << QStringLiteral("https://h/x");
    QTest::newRow("token as user") << QStringLiteral("https://ghp_abc@h/x")
                                   << QStringLiteral("https://h/x");
    QTest::newRow("references") << QStringLiteral("https://{{u}}:{{p}}@h/x")
                                << QStringLiteral("https://{{u}}:{{p}}@h/x");
    QTest::newRow("literal password") << QStringLiteral("https://{{u}}:pw@h/x")
                                      << QStringLiteral("https://{{u}}:@h/x");
    QTest::newRow("query") << QStringLiteral("http://h/x?access_token=T&page=2&api_key=K")
                           << QStringLiteral("http://h/x?access_token=&page=2&api_key=");
    QTest::newRow("query reference") << QStringLiteral("http://h/x?token={{t}}")
                                     << QStringLiteral("http://h/x?token={{t}}");
    QTest::newRow("before fragment") << QStringLiteral("http://h/x?token=T#top")
                                     << QStringLiteral("http://h/x?token=#top");
    QTest::newRow("variable host") << QStringLiteral("{{baseUrl}}/items?secret=S")
                                   << QStringLiteral("{{baseUrl}}/items?secret=");
    QTest::newRow("at in path") << QStringLiteral("http://h/a@b") << QStringLiteral("http://h/a@b");
    QTest::newRow("at in query") << QStringLiteral("http://h/x?mail=a@b")
                                 << QStringLiteral("http://h/x?mail=a@b");
    QTest::newRow("plain") << QStringLiteral("http://h:8080/x") << QStringLiteral("http://h:8080/x");
    QTest::newRow("no scheme") << QStringLiteral("admin:pw@example.com/x")
                               << QStringLiteral("example.com/x");
    QTest::newRow("no scheme, port") << QStringLiteral("example.com:8080/x")
                                     << QStringLiteral("example.com:8080/x");
    QTest::newRow("no scheme, reference") << QStringLiteral("{{baseUrl}}/x")
                                          << QStringLiteral("{{baseUrl}}/x");
    QTest::newRow("mixed user") << QStringLiteral("https://tok{{x}}@h/y")
                                << QStringLiteral("https://h/y");
}

void HistoryStoreTest::redactsUrlCredentials()
{
    QFETCH(QString, url);
    QFETCH(QString, expected);

    RequestSpec spec;
    spec.url = url;

    QCOMPARE(HistoryStore::redacted(spec).url, expected);
}

void HistoryStoreTest::redactsCredentialRows()
{
    RequestSpec spec;
    spec.queryParams = {{QStringLiteral("access_token"), QStringLiteral("L"), true},
                        {QStringLiteral("key"), QStringLiteral("{{k}}"), true}};
    spec.headers = {{QStringLiteral("X-Access-Token"), QStringLiteral("L"), true},
                    {QStringLiteral("X-Session-Id"), QStringLiteral("L"), false}};
    spec.body.form = {{QStringLiteral("Password"), QStringLiteral("L"), true},
                      {QStringLiteral("client_secret"), QStringLiteral("L"), true}};

    const RequestSpec result = HistoryStore::redacted(spec);

    QVERIFY(result.queryParams.at(0).value.isEmpty());
    QCOMPARE(result.queryParams.at(1).value, QStringLiteral("{{k}}"));
    QVERIFY(result.headers.at(0).value.isEmpty());
    QVERIFY(result.headers.at(1).value.isEmpty());
    QVERIFY(result.body.form.at(0).value.isEmpty());
    QVERIFY(result.body.form.at(1).value.isEmpty());
}

void HistoryStoreTest::redactsCommonCredentialNames_data()
{
    QTest::addColumn<QString>("name");

    QTest::newRow("key") << QStringLiteral("key");
    QTest::newRow("auth") << QStringLiteral("auth");
    QTest::newRow("pass") << QStringLiteral("pass");
    QTest::newRow("sig") << QStringLiteral("sig");
    QTest::newRow("signature") << QStringLiteral("X-Signature");
    QTest::newRow("access key") << QStringLiteral("aws_access_key");
    QTest::newRow("jwt") << QStringLiteral("jwt");
    QTest::newRow("x-auth") << QStringLiteral("X-Auth");
    QTest::newRow("authentication") << QStringLiteral("Authentication");
}

void HistoryStoreTest::redactsCommonCredentialNames()
{
    QFETCH(QString, name);

    RequestSpec spec;
    spec.queryParams = {{name, QStringLiteral("literal"), true}};

    QVERIFY(HistoryStore::redacted(spec).queryParams.at(0).value.isEmpty());
}

void HistoryStoreTest::redactsHeaderValuesWithAuthScheme()
{
    RequestSpec spec;
    spec.headers = {{QStringLiteral("X-Whatever"), QStringLiteral("Bearer abc"), true},
                    {QStringLiteral("X-Other"), QStringLiteral("basic abc"), true},
                    {QStringLiteral("X-Reference"), QStringLiteral("Bearer {{t}}"), true},
                    {QStringLiteral("X-Word"), QStringLiteral("Token"), true}};

    const RequestSpec result = HistoryStore::redacted(spec);

    QVERIFY(result.headers.at(0).value.isEmpty());
    QVERIFY(result.headers.at(1).value.isEmpty());
    QCOMPARE(result.headers.at(2).value, QStringLiteral("Bearer {{t}}"));
    QCOMPARE(result.headers.at(3).value, QStringLiteral("Token"));
}

void HistoryStoreTest::redactsTokenInUsername()
{
    RequestSpec spec;
    spec.auth.type = AuthType::Basic;
    spec.auth.username = QStringLiteral("sk_live_x");

    QVERIFY(HistoryStore::redacted(spec).auth.username.isEmpty());

    spec.auth.password = QStringLiteral("pw");
    QCOMPARE(HistoryStore::redacted(spec).auth.username, QStringLiteral("sk_live_x"));

    spec.auth.password.clear();
    spec.auth.username = QStringLiteral("{{user}}");
    QCOMPARE(HistoryStore::redacted(spec).auth.username, QStringLiteral("{{user}}"));
}

void HistoryStoreTest::redactsRawBody_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("expected");

    QTest::newRow("json") << QStringLiteral(R"({"user":"bob","password":"x"})")
                          << QStringLiteral(R"({"user":"bob","password":""})");
    QTest::newRow("nested") << QStringLiteral(R"({"a":{"b":[{"api_key":"x"}]},"n":1})")
                            << QStringLiteral(R"({"a":{"b":[{"api_key":""}]},"n":1})");
    QTest::newRow("array") << QStringLiteral(R"([{"token":"x"}])")
                           << QStringLiteral(R"([{"token":""}])");
    QTest::newRow("reference") << QStringLiteral(R"({"password":"{{pw}}"})")
                               << QStringLiteral(R"({"password":"{{pw}}"})");
    QTest::newRow("number") << QStringLiteral(R"({"otp_token":123456})")
                            << QStringLiteral(R"({"otp_token":""})");
    QTest::newRow("formatting") << QStringLiteral("{ \"a\" : 1,\n  \"password\" : \"x\" }")
                                << QStringLiteral("{ \"a\" : 1,\n  \"password\" : \"\" }");
    QTest::newRow("escaped quote") << QStringLiteral(R"({"password":"a\"b","n":"c"})")
                                   << QStringLiteral(R"({"password":"","n":"c"})");
    QTest::newRow("ordinary json") << QStringLiteral(R"({"user":"bob","age":7})")
                                   << QStringLiteral(R"({"user":"bob","age":7})");
    QTest::newRow("form") << QStringLiteral("user=bob&token=abc")
                          << QStringLiteral("user=bob&token=");
    QTest::newRow("form reference") << QStringLiteral("token={{t}}&a=b")
                                    << QStringLiteral("token={{t}}&a=b");
    QTest::newRow("xml") << QStringLiteral("<password>x</password>")
                         << QStringLiteral("<password>x</password>");
    QTest::newRow("text") << QStringLiteral("hello world") << QStringLiteral("hello world");
}

void HistoryStoreTest::redactsRawBody()
{
    QFETCH(QString, raw);
    QFETCH(QString, expected);

    RequestSpec spec;
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = raw;

    QCOMPARE(HistoryStore::redacted(spec).body.raw, expected);
}

void HistoryStoreTest::redactsLongRawBody()
{
    const QString filler(HistoryStore::kMaxStoredBodyChars - 40, u'a');
    RequestSpec spec;
    spec.body.raw = QStringLiteral(R"({"a":"%1","password":"x"})").arg(filler);

    QCOMPARE(HistoryStore::redacted(spec).body.raw,
             QStringLiteral(R"({"a":"%1","password":""})").arg(filler));
}

void HistoryStoreTest::keepsOrdinaryRows()
{
    RequestSpec spec;
    spec.queryParams = {{QStringLiteral("author"), QStringLiteral("Ann"), true},
                        {QStringLiteral("monkey"), QStringLiteral("Bob"), true}};
    spec.headers = {{QStringLiteral("Accept"), QStringLiteral("*/*"), true}};
    spec.body.form = {{QStringLiteral("name"), QStringLiteral("Bob"), true}};

    QVERIFY(HistoryStore::redacted(spec) == spec);
}

void HistoryStoreTest::redactsPartlyLiteralValues_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("expected");

    QTest::newRow("literal prefix") << QStringLiteral("Bearer LIT{{x}}") << QString();
    QTest::newRow("literal suffix") << QStringLiteral("{{x}}-LIT") << QString();
    QTest::newRow("bearer") << QStringLiteral("Bearer {{t}}") << QStringLiteral("Bearer {{t}}");
    QTest::newRow("basic") << QStringLiteral("basic {{t}}") << QStringLiteral("basic {{t}}");
    QTest::newRow("two references") << QStringLiteral("{{a}}{{b}}") << QStringLiteral("{{a}}{{b}}");
}

void HistoryStoreTest::redactsPartlyLiteralValues()
{
    QFETCH(QString, value);
    QFETCH(QString, expected);

    RequestSpec spec;
    spec.headers = {{QStringLiteral("Authorization"), value, true}};
    spec.auth.token = value;

    const RequestSpec result = HistoryStore::redacted(spec);

    QCOMPARE(result.headers.at(0).value, expected);
    QCOMPARE(result.auth.token, expected);
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
                      "{\"id\":\"d\",\"timestamp\":\"2026-11-04T12:00:00.000Z\"},"
                      "7]}"));

    HistoryStore store(path);
    QVERIFY(store.load());
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().at(0).request.url, QStringLiteral("http://ok"));
}

void HistoryStoreTest::addBeforeLoadKeepsExistingEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    {
        HistoryStore store(path);
        QVERIFY(store.add(makeEntry(QStringLiteral("http://old"))));
    }

    HistoryStore store(path);
    QVERIFY(store.add(makeEntry(QStringLiteral("http://new"))));
    QCOMPARE(store.entries().size(), 2);

    HistoryStore reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.entries().size(), 2);
    QCOMPARE(reloaded.entries().at(1).request.url, QStringLiteral("http://old"));
}

void HistoryStoreTest::unreadableHistoryIsNotOverwritten()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    {
        HistoryStore store(path);
        QVERIFY(store.add(makeEntry(QStringLiteral("http://old"))));
    }
    const QByteArray content = readFile(path);

    // A directory at the file's path cannot be opened, like a locked file.
    QVERIFY(QFile::remove(path));
    QVERIFY(QDir().mkpath(path));

    HistoryStore store(path);
    QString error;
    QVERIFY(!store.load(&error));
    QVERIFY(!store.add(makeEntry(QStringLiteral("http://while-locked")), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(store.entries().size(), 1);

    QVERIFY(QDir().rmdir(path));
    QVERIFY(writeFile(path, content));

    QVERIFY(store.add(makeEntry(QStringLiteral("http://after"))));
    QCOMPARE(store.entries().size(), 3);
    QCOMPARE(store.entries().at(0).request.url, QStringLiteral("http://after"));
    QCOMPARE(store.entries().at(2).request.url, QStringLiteral("http://old"));

    HistoryStore reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.entries().size(), 3);
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

void HistoryStoreTest::twoStoresKeepBothEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    HistoryStore first(path);
    HistoryStore second(path);
    QVERIFY(first.load());
    QVERIFY(second.load());

    QVERIFY(first.add(makeEntry(QStringLiteral("http://first"))));
    QVERIFY(second.add(makeEntry(QStringLiteral("http://second"))));

    QCOMPARE(second.entries().size(), 2);
    HistoryStore reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.entries().size(), 2);
    QCOMPARE(reloaded.entries().at(0).request.url, QStringLiteral("http://second"));
    QCOMPARE(reloaded.entries().at(1).request.url, QStringLiteral("http://first"));
}

void HistoryStoreTest::clearAfterOtherStoreAdded()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    HistoryStore first(path);
    HistoryStore second(path);
    QVERIFY(first.add(makeEntry(QStringLiteral("http://a"))));
    QVERIFY(second.add(makeEntry(QStringLiteral("http://b"))));

    QVERIFY(first.clear());
    QVERIFY(second.add(makeEntry(QStringLiteral("http://c"))));

    HistoryStore reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.entries().size(), 1);
    QCOMPARE(reloaded.entries().at(0).request.url, QStringLiteral("http://c"));
    QCOMPARE(second.entries().size(), 1);
}

void HistoryStoreTest::prepareAddReportsMerge()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    HistoryStore first(path);
    QVERIFY(first.add(makeEntry(QStringLiteral("http://a"))));

    HistoryStore::Update alone = first.prepareAdd(makeEntry(QStringLiteral("http://b")));
    QVERIFY(alone.ok);
    QVERIFY(alone.prependsOne);
    first.apply(std::move(alone));

    QVERIFY(HistoryStore(path).add(makeEntry(QStringLiteral("http://elsewhere"))));
    HistoryStore::Update merged = first.prepareAdd(makeEntry(QStringLiteral("http://c")));
    QVERIFY(merged.ok);
    QVERIFY(!merged.prependsOne);
    first.apply(std::move(merged));
    QCOMPARE(first.entries().size(), 4);
    QCOMPARE(first.entries().at(0).request.url, QStringLiteral("http://c"));
}

void HistoryStoreTest::longValuesAreCapped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    const QString huge(3 * HistoryStore::kMaxStoredValueChars, u'x');
    HistoryEntry entry = makeEntry(QStringLiteral("http://a/") + huge);
    entry.request.headers = {{QStringLiteral("X-Long"), huge, true}};
    entry.request.queryParams = {{huge, QStringLiteral("v"), true}};
    entry.request.body.form = {{QStringLiteral("f"), huge, true}};
    entry.errorString = huge;

    HistoryStore store(path);
    QVERIFY(store.add(entry));

    HistoryStore reloaded(path);
    QVERIFY(reloaded.load());
    const HistoryEntry &stored = reloaded.entries().at(0);
    QCOMPARE(stored.request.url.size(), HistoryStore::kMaxStoredValueChars);
    QVERIFY(stored.request.url.startsWith(QStringLiteral("http://a/x")));
    QCOMPARE(stored.request.headers.at(0).value.size(), HistoryStore::kMaxStoredValueChars);
    QCOMPARE(stored.request.queryParams.at(0).key.size(), HistoryStore::kMaxStoredValueChars);
    QCOMPARE(stored.request.body.form.at(0).value.size(), HistoryStore::kMaxStoredValueChars);
    QCOMPARE(stored.errorString.size(), HistoryStore::kMaxStoredValueChars);
    QVERIFY(readFile(path).size() < 8 * HistoryStore::kMaxStoredValueChars);
}

void HistoryStoreTest::clearRemovesCorruptFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    QVERIFY(writeFile(path, "{oops"));
    HistoryStore store(path);
    QVERIFY(!store.load());
    QVERIFY(QFile::exists(path + QStringLiteral(".corrupt")));

    QVERIFY(store.clear());

    QVERIFY(!QFile::exists(path));
    QVERIFY(!QFile::exists(path + QStringLiteral(".corrupt")));
}

void HistoryStoreTest::clearWhileLockedKeepsEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.json"));
    HistoryStore store(path);
    QVERIFY(store.add(makeEntry(QStringLiteral("http://a"))));
    QLockFile other(path + QStringLiteral(".lock"));
    QVERIFY(other.tryLock(0));

    QString error;
    QVERIFY(!store.clear(&error));

    QVERIFY(!error.isEmpty());
    QCOMPARE(store.entries().size(), 1);
    QVERIFY(QFile::exists(path));
}

QTEST_GUILESS_MAIN(HistoryStoreTest)

#include "tst_historystore.moc"
