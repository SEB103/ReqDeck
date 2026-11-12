// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "core/import/insomniaimporter.h"

/*!
 * \internal
 * \brief Returns the Insomnia export of the test data.
 */
static QByteArray sampleExport()
{
    QFile file(QFINDTESTDATA("data/import/insomnia-shop.json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/*! Verifies the import of Insomnia exports. */
class InsomniaImporterTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that the format is recognized. */
    void recognizesFormat();

    /*! Verifies folders, requests, parameters, headers, bodies, and references. */
    void importsStructureAndRequests();

    /*! Verifies auth, the base environment as folder variables, and environments. */
    void importsAuthAndEnvironments();

    /*! Verifies that scripts, template tags, and unsupported parts are reported. */
    void reportsWhatIsDropped();

    /*! Verifies that other files are refused. */
    void rejectsOtherFiles();

    /*! Verifies that a folder inside itself and a very long chain of folders end with a report line. */
    void stopsLoopsAndDeepChains();
};

/*!
 * \internal
 * \brief Returns an export of workspace wrk with \a resources after it.
 */
static QByteArray exportWith(const QByteArray &resources)
{
    return "{\"_type\": \"export\", \"__export_format\": 4, \"resources\": ["
           "{\"_id\": \"wrk\", \"_type\": \"workspace\", \"name\": \"W\"}"
           + resources + "]}";
}

void InsomniaImporterTest::recognizesFormat()
{
    QVERIFY(InsomniaImporter::isExport(QJsonDocument::fromJson(sampleExport()).object()));
    QVERIFY(!InsomniaImporter::isExport(QJsonDocument::fromJson("{\"_type\": \"export\"}").object()));
}

void InsomniaImporterTest::importsStructureAndRequests()
{
    const ImportResult result = InsomniaImporter::importExport(sampleExport());

    QVERIFY2(result.ok, qPrintable(result.errorString));
    QVERIFY(result.hasCollection);
    QCOMPARE(result.folder.name, QStringLiteral("Shop"));
    QCOMPARE(result.folder.children.size(), 4);
    const CollectionNode &orders = result.folder.children.at(0);
    QCOMPARE(orders.kind, CollectionNode::Kind::Folder);
    QCOMPARE(orders.name, QStringLiteral("Orders"));
    QCOMPARE(orders.children.size(), 2);

    const RequestSpec &list = orders.children.at(0).request;
    QCOMPARE(list.url, QStringLiteral("{{baseUrl}}/orders"));
    QCOMPARE(list.queryParams.size(), 2);
    QVERIFY(!list.queryParams.at(1).enabled);
    QCOMPARE(list.headers.at(0).key, QStringLiteral("Accept"));
    QCOMPARE(list.auth.type, AuthType::Inherit);

    const RequestSpec &create = orders.children.at(1).request;
    QCOMPARE(create.body.mode, BodyMode::Raw);
    QCOMPARE(create.body.rawType, RawBodyType::Json);
    QCOMPARE(create.auth.type, AuthType::None);

    const RequestSpec &login = result.folder.children.at(1).request;
    QCOMPARE(login.body.mode, BodyMode::FormUrlEncoded);
    QCOMPARE(login.body.form.at(0).value, QStringLiteral("{{user}}"));
}

void InsomniaImporterTest::importsAuthAndEnvironments()
{
    const ImportResult result = InsomniaImporter::importExport(sampleExport());

    QCOMPARE(result.folder.children.at(0).folder.auth.type, AuthType::Bearer);
    QCOMPARE(result.folder.children.at(0).folder.auth.token, QStringLiteral("{{token}}"));
    const RequestAuth &basic = result.folder.children.at(1).request.auth;
    QCOMPARE(basic.type, AuthType::Basic);
    QCOMPARE(basic.password, QStringLiteral("{{pass}}"));
    const RequestAuth &key = result.folder.children.at(3).request.auth;
    QCOMPARE(key.type, AuthType::ApiKey);
    QCOMPARE(key.apiKeyLocation, ApiKeyLocation::Query);

    QCOMPARE(result.folder.folder.variables.size(), 1);
    QCOMPARE(result.folder.folder.variables.at(0).key, QStringLiteral("baseUrl"));
    QCOMPARE(result.environments.size(), 1);
    const Environment &dev = result.environments.at(0);
    QCOMPARE(dev.name, QStringLiteral("Dev"));
    QCOMPARE(dev.variables.size(), 2);
    // QJsonObject keeps keys sorted, so "limits" comes before "token".
    QCOMPARE(dev.variables.at(0).key, QStringLiteral("limits"));
    QCOMPARE(dev.variables.at(0).value, QStringLiteral("{\"page\":10}"));
    QCOMPARE(dev.variables.at(1).value, QStringLiteral("dev-token"));
}

void InsomniaImporterTest::reportsWhatIsDropped()
{
    const ImportResult result = InsomniaImporter::importExport(sampleExport());
    const QString report = result.report.join(u'\n');

    QVERIFY2(report.contains(QStringLiteral("Shop / Orders / Create order: scripts")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Create order: template tag {% uuid 'v4' %}")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Upload: oauth2 authorization")), qPrintable(report));
    QVERIFY2(report.contains(QStringLiteral("Upload: field f has no file; choose one")), qPrintable(report));
    const RequestSpec &upload = result.folder.children.at(2).request;
    QCOMPARE(upload.body.mode, BodyMode::FormData);
    QCOMPARE(upload.body.parts.size(), 1);
    QVERIFY(upload.body.parts.at(0).isFile);
    QCOMPARE(result.folder.children.at(2).request.auth.type, AuthType::None);
}

void InsomniaImporterTest::rejectsOtherFiles()
{
    QVERIFY(!InsomniaImporter::importExport("{oops").ok);
    QVERIFY(!InsomniaImporter::importExport("{\"_type\": \"export\", \"resources\": []}").ok);
}

void InsomniaImporterTest::stopsLoopsAndDeepChains()
{
    // fld_a and fld_b are each other's parent, below a group that names the workspace as itself.
    const ImportResult loop = InsomniaImporter::importExport(exportWith(
        ",{\"_id\": \"wrk\", \"_type\": \"request_group\", \"parentId\": \"wrk\", \"name\": \"Self\"}"
        ",{\"_id\": \"fld_a\", \"_type\": \"request_group\", \"parentId\": \"wrk\", \"name\": \"A\"}"
        ",{\"_id\": \"fld_b\", \"_type\": \"request_group\", \"parentId\": \"fld_a\", \"name\": \"B\"}"
        ",{\"_id\": \"fld_a\", \"_type\": \"request_group\", \"parentId\": \"fld_b\", \"name\": \"A again\"}"));
    QVERIFY(loop.ok);
    QVERIFY2(loop.report.join(u'\n').contains(QStringLiteral("folder inside itself")), qPrintable(loop.report.join(u'\n')));

    QByteArray chain;
    QByteArray parent = "wrk";
    for (int i = 0; i < 5000; ++i) {
        const QByteArray id = "fld_" + QByteArray::number(i);
        chain += ",{\"_id\": \"" + id + "\", \"_type\": \"request_group\", \"parentId\": \"" + parent
                 + "\", \"name\": \"F\"}";
        parent = id;
    }
    const ImportResult deep = InsomniaImporter::importExport(exportWith(chain));
    QVERIFY(deep.ok);
    QVERIFY2(deep.report.join(u'\n').contains(QStringLiteral("nested deeper than")), "no report line");
    int height = 1;
    for (const CollectionNode *node = &deep.folder; !node->children.isEmpty(); node = &node->children.first())
        ++height;
    QVERIFY(height <= kMaxCollectionDepth);
}

QTEST_GUILESS_MAIN(InsomniaImporterTest)

#include "tst_insomniaimporter.moc"
