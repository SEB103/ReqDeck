// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "core/import/postmanimporter.h"

/*!
 * \internal
 * \brief Returns the content of the test data file \a name.
 */
#define readData(name) readFile(QFINDTESTDATA(name))

/*!
 * \internal
 * \brief Returns the content of the file at \a path, or an empty array.
 */
static QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/*!
 * \internal
 * \brief Returns a Postman 2.1 collection with what ReqDeck carries over and what it drops.
 */
static QByteArray sampleCollection()
{
    QFile file(QFINDTESTDATA("data/import/postman-shop.postman_collection.json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/*! Verifies the import of Postman collections and environments. */
class PostmanImporterTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that the formats are told apart. */
    void recognizesFormats();

    /*! Verifies folders, requests, URLs, headers, and bodies. */
    void importsStructureAndRequests();

    /*! Verifies auth at the collection, folder, and request level, and the collection variables. */
    void importsAuthAndVariables();

    /*! Verifies that scripts, examples, unsupported auth and bodies, and dynamic variables are reported, not kept. */
    void scriptsAreReportedNotKept();

    /*! Verifies environments, with secrets kept as secret. */
    void importsEnvironment();

    /*! Verifies that a file that is no collection fails with a message. */
    void rejectsOtherFiles();

    /*! Verifies that Collection v2.0 auth, attributes as one object, is read. */
    void readsVersion20Auth();

    /*! Verifies that folders nested deeper than ReqDeck allows are left out and reported. */
    void leavesOutTooDeepFolders();
};

void PostmanImporterTest::recognizesFormats()
{
    QVERIFY(PostmanImporter::isCollection(QJsonDocument::fromJson(sampleCollection()).object()));
    QVERIFY(!PostmanImporter::isEnvironment(QJsonDocument::fromJson(sampleCollection()).object()));
    const QJsonObject environment =
        QJsonDocument::fromJson(readData("data/import/staging.postman_environment.json")).object();
    QVERIFY(PostmanImporter::isEnvironment(environment));
    QVERIFY(!PostmanImporter::isCollection(environment));
}

void PostmanImporterTest::importsStructureAndRequests()
{
    const ImportResult result = PostmanImporter::importCollection(sampleCollection());

    QVERIFY2(result.ok, qPrintable(result.errorString));
    QVERIFY(result.hasCollection);
    QCOMPARE(result.folder.name, QStringLiteral("Shop API"));
    QCOMPARE(result.folder.children.size(), 4);
    const CollectionNode &orders = result.folder.children.at(0);
    QCOMPARE(orders.kind, CollectionNode::Kind::Folder);
    QCOMPARE(orders.children.size(), 2);
    QVERIFY(isValidWorkspaceId(orders.id) || !orders.id.isEmpty());

    const RequestSpec &list = orders.children.at(0).request;
    QCOMPARE(orders.children.at(0).name, QStringLiteral("List orders"));
    QCOMPARE(list.method, HttpMethod::Get);
    QCOMPARE(list.url, QStringLiteral("{{baseUrl}}/orders"));
    QCOMPARE(list.queryParams.size(), 2);
    QCOMPARE(list.queryParams.at(1).key, QStringLiteral("size"));
    QCOMPARE(list.headers.size(), 2);
    QVERIFY(!list.headers.at(1).enabled);
    QCOMPARE(list.auth.type, AuthType::Inherit);

    const RequestSpec &create = orders.children.at(1).request;
    QCOMPARE(create.url, QStringLiteral("{{baseUrl}}/orders"));
    QCOMPARE(create.body.mode, BodyMode::Raw);
    QCOMPARE(create.body.rawType, RawBodyType::Json);
    QCOMPARE(create.auth.type, AuthType::None);

    const RequestSpec &login = result.folder.children.at(1).request;
    QCOMPARE(login.body.mode, BodyMode::FormUrlEncoded);
    QCOMPARE(login.body.form.at(0).key, QStringLiteral("remember"));
}

void PostmanImporterTest::importsAuthAndVariables()
{
    const ImportResult result = PostmanImporter::importCollection(sampleCollection());

    QCOMPARE(result.folder.folder.auth.type, AuthType::Bearer);
    QCOMPARE(result.folder.folder.auth.token, QStringLiteral("{{token}}"));
    QCOMPARE(result.folder.folder.variables.size(), 2);
    QCOMPARE(result.folder.folder.variables.at(0).key, QStringLiteral("baseUrl"));
    QVERIFY(!result.folder.folder.variables.at(1).enabled);
    QCOMPARE(result.folder.children.at(0).folder.auth.type, AuthType::Inherit);

    const RequestAuth &basic = result.folder.children.at(1).request.auth;
    QCOMPARE(basic.type, AuthType::Basic);
    QCOMPARE(basic.username, QStringLiteral("{{user}}"));
    QCOMPARE(basic.password, QStringLiteral("{{pass}}"));

    const RequestAuth &key = result.folder.children.at(3).request.auth;
    QCOMPARE(key.type, AuthType::ApiKey);
    QCOMPARE(key.apiKeyName, QStringLiteral("X-Key"));
    QCOMPARE(key.apiKeyValue, QStringLiteral("{{key}}"));
    QCOMPARE(key.apiKeyLocation, ApiKeyLocation::Query);
}

void PostmanImporterTest::scriptsAreReportedNotKept()
{
    const ImportResult result = PostmanImporter::importCollection(sampleCollection());
    const QString report = result.report.join(u'\n');

    QVERIFY2(report.contains(QStringLiteral("Shop API: scripts")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Shop API / Orders / List orders: scripts")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("List orders: saved example responses")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Upload: OAuth 2.0")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Upload: file a.png is a path of the computer the file came from")), qPrintable(report));
    const RequestSpec &upload = result.folder.children.at(2).request;
    QCOMPARE(upload.body.mode, BodyMode::FormData);
    QCOMPARE(upload.body.parts.size(), 1);
    QVERIFY(upload.body.parts.at(0).isFile);
    QCOMPARE(upload.body.parts.at(0).value, QStringLiteral("a.png"));
    QVERIFY2(report.contains(QStringLiteral("Create order: dynamic variable {{$guid}}")), qPrintable(report));
    QCOMPARE(result.folder.children.at(2).request.auth.type, AuthType::None);
    QCOMPARE(result.folder.children.at(2).request.body.mode, BodyMode::FormData);
    QVERIFY(!QJsonDocument::fromJson(sampleCollection()).isNull());
}

void PostmanImporterTest::importsEnvironment()
{
    const ImportResult result = PostmanImporter::importEnvironment(readData("data/import/staging.postman_environment.json"));

    QVERIFY2(result.ok, qPrintable(result.errorString));
    QVERIFY(!result.hasCollection);
    QCOMPARE(result.environments.size(), 1);
    const Environment &environment = result.environments.at(0);
    QCOMPARE(environment.name, QStringLiteral("Staging"));
    QVERIFY(!environment.id.isEmpty());
    QCOMPARE(environment.variables.size(), 3);
    QVERIFY(!environment.variables.at(0).secret);
    QVERIFY(environment.variables.at(1).secret);
    QCOMPARE(environment.variables.at(1).value, QStringLiteral("abc"));
    QVERIFY(!environment.variables.at(2).enabled);
}

void PostmanImporterTest::rejectsOtherFiles()
{
    for (const QByteArray &json : {QByteArray("{oops"), QByteArray("[]"), QByteArray("{\"info\": {\"name\": \"x\"}}")}) {
        const ImportResult result = PostmanImporter::importCollection(json);
        QVERIFY(!result.ok);
        QVERIFY(!result.errorString.isEmpty());
    }
    QVERIFY(!PostmanImporter::importEnvironment("{}").ok);
}

void PostmanImporterTest::readsVersion20Auth()
{
    const QByteArray json = "{\"info\": {\"name\": \"Old\", \"schema\": "
                            "\"https://schema.getpostman.com/json/collection/v2.0.0/collection.json\"},"
                            "\"auth\": {\"type\": \"bearer\", \"bearer\": {\"token\": \"{{tok}}\"}},"
                            "\"item\": [{\"name\": \"Get\", \"request\": {\"method\": \"GET\", \"url\": \"https://h/\","
                            "\"auth\": {\"type\": \"basic\", \"basic\": {\"username\": \"u\", \"password\": \"p\"}}}}]}";
    const ImportResult result = PostmanImporter::importCollection(json);

    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.folder.folder.auth.token, QStringLiteral("{{tok}}"));
    QCOMPARE(result.folder.children.at(0).request.auth.username, QStringLiteral("u"));
    QCOMPARE(result.folder.children.at(0).request.auth.password, QStringLiteral("p"));
}

void PostmanImporterTest::leavesOutTooDeepFolders()
{
    QByteArray items = "{\"name\": \"Leaf\", \"request\": {\"method\": \"GET\", \"url\": \"https://h/\"}}";
    for (int i = 0; i < kMaxCollectionDepth + 5; ++i)
        items = "{\"name\": \"F\", \"item\": [" + items + "]}";
    const QByteArray json = "{\"info\": {\"name\": \"Deep\", \"schema\": "
                            "\"https://schema.getpostman.com/json/collection/v2.1.0/collection.json\"}, \"item\": ["
                            + items + "]}";
    const ImportResult result = PostmanImporter::importCollection(json);

    QVERIFY2(result.ok, qPrintable(result.errorString));
    QVERIFY(result.report.join(u'\n').contains(QStringLiteral("nested deeper than")));
    int height = 1;
    for (const CollectionNode *node = &result.folder; !node->children.isEmpty(); node = &node->children.first())
        ++height;
    QVERIFY(height <= kMaxCollectionDepth);
}

QTEST_GUILESS_MAIN(PostmanImporterTest)

#include "tst_postmanimporter.moc"
