// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "core/workspace/workspaceserializer.h"

/*!
 * \internal
 * \brief Builds a fully populated WorkspaceData for round-trip tests.
 */
static WorkspaceData makeWorkspace()
{
    WorkspaceData data;
    data.displayName = QStringLiteral("Billing API");
    data.collection = QJsonArray {
        QJsonObject {{QStringLiteral("id"), QStringLiteral("f1")},
                     {QStringLiteral("kind"), QStringLiteral("folder")},
                     {QStringLiteral("name"), QStringLiteral("Invoices")}}};
    data.environments = QJsonArray {
        QJsonObject {{QStringLiteral("id"), QStringLiteral("e1")},
                     {QStringLiteral("name"), QStringLiteral("Staging")}}};
    data.activeEnvironmentId = QStringLiteral("e1");
    data.settings = QJsonObject {{QStringLiteral("timeoutMs"), 30000}};
    return data;
}

/*!
 * \internal
 * \brief Writes \a content to \a path, replacing any existing file.
 */
static bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(content) == content.size();
}

/*! Verifies WorkspaceSerializer round-trip and validation behavior. */
class WorkspaceSerializerTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that saving then loading preserves every workspace field. */
    void roundTripPreservesData();

    /*! Verifies that the written file stores the schema version under "schema". */
    void savedFileCarriesSchemaVersion();

    /*! Verifies that a file with only the schema version loads with empty defaults. */
    void minimalFileLoadsWithDefaults();

    /*! Verifies that loading a missing file reports FileNotFound. */
    void loadMissingFileReportsError();

    /*! Verifies that loading malformed JSON reports InvalidJson. */
    void loadInvalidJsonReportsError();

    /*! Verifies that a JSON array root reports InvalidJson. */
    void loadNonObjectRootReportsError();

    /*! Verifies that a missing or non-integer schema reports InvalidSchema. */
    void loadInvalidSchemaMemberReportsError_data();
    /*! Runs one invalid-schema case. */
    void loadInvalidSchemaMemberReportsError();

    /*! Verifies that unsupported schema versions report UnsupportedVersion. */
    void loadUnsupportedVersionReportsError_data();
    /*! Runs one unsupported-version case. */
    void loadUnsupportedVersionReportsError();

    /*! Verifies that an optional member with the wrong JSON type reports InvalidSchema. */
    void loadWrongMemberTypeReportsError_data();
    /*! Runs one wrong-member-type case. */
    void loadWrongMemberTypeReportsError();

    /*! Verifies that saving creates missing parent directories. */
    void saveCreatesParentDirectories();
};

void WorkspaceSerializerTest::roundTripPreservesData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("billing.reqdeck"));

    const WorkspaceData original = makeWorkspace();
    QString error;
    QVERIFY2(WorkspaceSerializer::save(path, original, &error), qPrintable(error));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.error, WorkspaceSerializer::Error::None);
    QCOMPARE(result.data.schemaVersion, kWorkspaceSchemaVersion);
    QVERIFY(result.data == original);
}

void WorkspaceSerializerTest::savedFileCarriesSchemaVersion()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("schema.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(path, makeWorkspace()));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(root.value(QStringLiteral("schema")).toInt(), kWorkspaceSchemaVersion);
    QCOMPARE(root.value(QStringLiteral("displayName")).toString(), QStringLiteral("Billing API"));
}

void WorkspaceSerializerTest::minimalFileLoadsWithDefaults()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("minimal.reqdeck"));
    QVERIFY(writeFile(path, R"({"schema": 1})"));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.data.schemaVersion, 1);
    QVERIFY(result.data.displayName.isEmpty());
    QVERIFY(result.data.collection.isEmpty());
    QVERIFY(result.data.environments.isEmpty());
    QVERIFY(result.data.activeEnvironmentId.isEmpty());
    QVERIFY(result.data.settings.isEmpty());
}

void WorkspaceSerializerTest::loadMissingFileReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const WorkspaceSerializer::LoadResult result =
        WorkspaceSerializer::load(dir.filePath(QStringLiteral("nope.reqdeck")));
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::FileNotFound);
    QVERIFY(!result.errorString.isEmpty());
}

void WorkspaceSerializerTest::loadInvalidJsonReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("broken.reqdeck"));
    QVERIFY(writeFile(path, "{ this is not json "));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::InvalidJson);
    QVERIFY(!result.errorString.isEmpty());
}

void WorkspaceSerializerTest::loadNonObjectRootReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("array.reqdeck"));
    QVERIFY(writeFile(path, "[1, 2, 3]"));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::InvalidJson);
}

void WorkspaceSerializerTest::loadInvalidSchemaMemberReportsError_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("missing") << QByteArray(R"({"displayName": "X"})");
    QTest::newRow("string") << QByteArray(R"({"schema": "1"})");
    QTest::newRow("fraction") << QByteArray(R"({"schema": 1.5})");
    QTest::newRow("null") << QByteArray(R"({"schema": null})");
}

void WorkspaceSerializerTest::loadInvalidSchemaMemberReportsError()
{
    QFETCH(QByteArray, content);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("invalid.reqdeck"));
    QVERIFY(writeFile(path, content));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::InvalidSchema);
}

void WorkspaceSerializerTest::loadUnsupportedVersionReportsError_data()
{
    QTest::addColumn<int>("version");
    QTest::newRow("zero") << 0;
    QTest::newRow("negative") << -1;
    QTest::newRow("future") << kWorkspaceSchemaVersion + 1;
}

void WorkspaceSerializerTest::loadUnsupportedVersionReportsError()
{
    QFETCH(int, version);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("version.reqdeck"));
    QVERIFY(writeFile(path, QStringLiteral(R"({"schema": %1})").arg(version).toUtf8()));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::UnsupportedVersion);
}

void WorkspaceSerializerTest::loadWrongMemberTypeReportsError_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("displayName") << QByteArray(R"({"schema": 1, "displayName": 5})");
    QTest::newRow("collection") << QByteArray(R"({"schema": 1, "collection": {}})");
    QTest::newRow("environments") << QByteArray(R"({"schema": 1, "environments": "x"})");
    QTest::newRow("activeEnvironmentId")
        << QByteArray(R"({"schema": 1, "activeEnvironmentId": []})");
    QTest::newRow("settings") << QByteArray(R"({"schema": 1, "settings": []})");
}

void WorkspaceSerializerTest::loadWrongMemberTypeReportsError()
{
    QFETCH(QByteArray, content);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("types.reqdeck"));
    QVERIFY(writeFile(path, content));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::InvalidSchema);
}

void WorkspaceSerializerTest::saveCreatesParentDirectories()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("a/b/nested.reqdeck"));

    QString error;
    QVERIFY2(WorkspaceSerializer::save(path, makeWorkspace(), &error), qPrintable(error));
    QVERIFY(WorkspaceSerializer::load(path).ok);
}

QTEST_GUILESS_MAIN(WorkspaceSerializerTest)

#include "tst_workspaceserializer.moc"
