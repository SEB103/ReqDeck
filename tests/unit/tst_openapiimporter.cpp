// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "core/import/openapiimporter.h"

/*!
 * \internal
 * \brief Returns the OpenAPI document of the test data.
 */
static QByteArray sampleDocument()
{
    QFile file(QFINDTESTDATA("data/import/openapi-petshop.json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/*!
 * \internal
 * \brief Returns the value of the variable \a key in \a rows, or "<missing>".
 */
static QString valueOf(const QList<KeyValue> &rows, const QString &key)
{
    for (const KeyValue &row : rows) {
        if (row.key == key)
            return row.value;
    }
    return QStringLiteral("<missing>");
}

/*! Verifies the import of OpenAPI 3 documents. */
class OpenApiImporterTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that OpenAPI 3 is recognized and Swagger 2.0 refused with a message. */
    void recognizesFormat();

    /*! Verifies tag folders, request names, URLs, and parameters. */
    void importsPathsByTag();

    /*! Verifies request bodies from examples and from schemas. */
    void importsBodies();

    /*! Verifies servers, path variables, and security schemes. */
    void importsServersAndSecurity();

    /*! Verifies that what ReqDeck cannot carry over is reported. */
    void reportsWhatIsDropped();
};

void OpenApiImporterTest::recognizesFormat()
{
    QVERIFY(OpenApiImporter::isDocument(QJsonDocument::fromJson(sampleDocument()).object()));
    const QByteArray swagger = "{\"swagger\": \"2.0\", \"paths\": {}}";
    QVERIFY(OpenApiImporter::isDocument(QJsonDocument::fromJson(swagger).object()));
    const ImportResult old = OpenApiImporter::importDocument(swagger);
    QVERIFY(!old.ok);
    QVERIFY2(old.errorString.contains(QStringLiteral("Swagger 2.0")), qPrintable(old.errorString));
    QVERIFY(!OpenApiImporter::isDocument(QJsonDocument::fromJson("{\"openapi\": \"3.0.0\"}").object()));
    QVERIFY(!OpenApiImporter::importDocument("{oops").ok);
}

void OpenApiImporterTest::importsPathsByTag()
{
    const ImportResult result = OpenApiImporter::importDocument(sampleDocument());

    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.format, QStringLiteral("OpenAPI document"));
    QCOMPARE(result.folder.name, QStringLiteral("Pet Shop"));
    QCOMPARE(result.folder.children.size(), 3);
    const CollectionNode &pets = result.folder.children.at(0);
    QCOMPARE(pets.kind, CollectionNode::Kind::Folder);
    QCOMPARE(pets.name, QStringLiteral("pets"));
    QCOMPARE(pets.children.size(), 4);
    QCOMPARE(result.folder.children.at(1).name, QStringLiteral("store"));
    QCOMPARE(result.folder.children.at(1).children.size(), 2);
    QCOMPARE(result.folder.children.at(2).name, QStringLiteral("Health"));
    QCOMPARE(result.folder.children.at(2).kind, CollectionNode::Kind::Request);

    const RequestSpec &list = pets.children.at(0).request;
    QCOMPARE(pets.children.at(0).name, QStringLiteral("List pets"));
    QCOMPARE(list.method, HttpMethod::Get);
    QCOMPARE(list.url, QStringLiteral("{{baseUrl}}/pets"));
    QCOMPARE(list.queryParams.size(), 2);
    QCOMPARE(list.queryParams.at(0).key, QStringLiteral("limit"));
    QCOMPARE(list.queryParams.at(0).value, QStringLiteral("20"));
    QVERIFY(!list.queryParams.at(0).enabled);
    QCOMPARE(list.queryParams.at(1).value, QStringLiteral("available"));
    QVERIFY(list.queryParams.at(1).enabled);
    QCOMPARE(list.headers.size(), 1);
    QCOMPARE(list.headers.at(0).key, QStringLiteral("X-Trace"));
    QCOMPARE(list.headers.at(0).value, QStringLiteral("abc"));

    QCOMPARE(pets.children.at(1).name, QStringLiteral("createPet"));
    QCOMPARE(pets.children.at(1).request.method, HttpMethod::Post);
    QCOMPARE(pets.children.at(1).request.headers.size(), 1);
    QCOMPARE(pets.children.at(2).request.url, QStringLiteral("{{baseUrl}}/pets/{{petId}}"));
    QCOMPARE(pets.children.at(3).request.method, HttpMethod::Delete);
}

void OpenApiImporterTest::importsBodies()
{
    const ImportResult result = OpenApiImporter::importDocument(sampleDocument());

    const RequestSpec &create = result.folder.children.at(0).children.at(1).request;
    QCOMPARE(create.body.mode, BodyMode::Raw);
    QCOMPARE(create.body.rawType, RawBodyType::Json);
    const QJsonObject pet = QJsonDocument::fromJson(create.body.raw.toUtf8()).object();
    QCOMPARE(pet.value(QLatin1StringView("name")).toString(), QStringLiteral("Rex"));
    QCOMPARE(pet.value(QLatin1StringView("age")).toInt(-1), 0);
    QCOMPARE(pet.value(QLatin1StringView("tags")).toArray().size(), 1);
    const QJsonObject owner = pet.value(QLatin1StringView("owner")).toObject();
    QVERIFY(owner.value(QLatin1StringView("vip")).isBool());
    QVERIFY(owner.contains(QLatin1StringView("email")));

    const RequestSpec &order = result.folder.children.at(1).children.at(0).request;
    QCOMPARE(order.body.mode, BodyMode::Raw);
    const QJsonObject example = QJsonDocument::fromJson(order.body.raw.toUtf8()).object();
    QCOMPARE(example.value(QLatin1StringView("quantity")).toInt(), 1);

    QCOMPARE(result.folder.children.at(1).children.at(1).request.body.mode, BodyMode::None);
}

void OpenApiImporterTest::importsServersAndSecurity()
{
    const ImportResult result = OpenApiImporter::importDocument(sampleDocument());
    const QList<KeyValue> &variables = result.folder.folder.variables;

    QCOMPARE(variables.at(0).key, QStringLiteral("baseUrl"));
    QCOMPARE(variables.at(0).value, QStringLiteral("https://eu.pets.example.com/v1"));
    QCOMPARE(valueOf(variables, QStringLiteral("petId")), QStringLiteral("7"));
    QCOMPARE(valueOf(variables, QStringLiteral("token")), QString());
    QCOMPARE(valueOf(variables, QStringLiteral("apiKey")), QString());

    QCOMPARE(result.environments.size(), 2);
    QCOMPARE(result.environments.at(0).name, QStringLiteral("Production"));
    QCOMPARE(result.environments.at(1).name, QStringLiteral("Local"));
    QCOMPARE(result.environments.at(1).variables.at(0).key, QStringLiteral("baseUrl"));
    QCOMPARE(result.environments.at(1).variables.at(0).value, QStringLiteral("http://localhost:8080/v1"));

    QCOMPARE(result.folder.folder.auth.type, AuthType::Bearer);
    QCOMPARE(result.folder.folder.auth.token, QStringLiteral("{{token}}"));
    const CollectionNode &pets = result.folder.children.at(0);
    QCOMPARE(pets.folder.auth.type, AuthType::Inherit);
    QCOMPARE(pets.children.at(0).request.auth.type, AuthType::Inherit);
    const RequestAuth &key = pets.children.at(3).request.auth;
    QCOMPARE(key.type, AuthType::ApiKey);
    QCOMPARE(key.apiKeyName, QStringLiteral("X-API-Key"));
    QCOMPARE(key.apiKeyValue, QStringLiteral("{{apiKey}}"));
    QCOMPARE(key.apiKeyLocation, ApiKeyLocation::Header);
    QCOMPARE(result.folder.children.at(2).request.auth.type, AuthType::None);
}

void OpenApiImporterTest::reportsWhatIsDropped()
{
    const ImportResult result = OpenApiImporter::importDocument(sampleDocument());
    const QString report = result.report.join(u'\n');

    QVERIFY2(report.contains(QStringLiteral("Pet Shop / pets / Get a pet: cookie parameter session")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Pet Shop / store / Place an order: oauth2 authorization")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Pet Shop / store / Upload a photo: multipart/form-data body")), qPrintable(report));
    QCOMPARE(result.folder.children.at(1).children.at(0).request.auth.type, AuthType::None);
}

QTEST_GUILESS_MAIN(OpenApiImporterTest)

#include "tst_openapiimporter.moc"
