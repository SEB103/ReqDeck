// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include "core/workspace/workspaceserializer.h"

/*!
 * \internal
 * \brief Builds a fully populated WorkspaceData for round-trip tests.
 */
static WorkspaceData makeWorkspace()
{
    CollectionNode request;
    request.kind = CollectionNode::Kind::Request;
    request.id = QStringLiteral("r1");
    request.name = QStringLiteral("List");
    request.request.url = QStringLiteral("{{baseUrl}}/invoices");
    request.request.auth.type = AuthType::Bearer;
    request.request.auth.token = QStringLiteral("{{token}}");

    CollectionNode folder;
    folder.id = QStringLiteral("f1");
    folder.name = QStringLiteral("Invoices");
    folder.children = {request};

    Environment environment;
    environment.id = QStringLiteral("e1");
    environment.name = QStringLiteral("Staging");
    environment.variables = {{QStringLiteral("baseUrl"), QStringLiteral("https://s"), true, false},
                             {QStringLiteral("off"), QStringLiteral("x"), false, false}};

    WorkspaceData data;
    data.id = QStringLiteral("3f2b8c1e-7a44-4a8e-9a55-0c1d2e3f4a5b");
    data.displayName = QStringLiteral("Billing API");
    data.collection = {folder};
    data.environments = {environment};
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

    /*! Verifies that a deep folder hierarchy survives a round trip. */
    void deepNestingRoundTrips();

    /*! Verifies that the value of a secret variable never reaches the file. */
    void secretValueIsNeverWritten();

    /*! Verifies that a secret value present in a file is kept in memory. */
    void secretValueInFileIsKept();

    /*! Verifies that only a UUID is accepted as workspace id. */
    void rejectsNonUuidWorkspaceId_data();
    /*! Runs one invalid workspace id case. */
    void rejectsNonUuidWorkspaceId();

    /*! Verifies the rejection of malformed collection items. */
    void rejectsInvalidCollection_data();
    /*! Runs one invalid collection case. */
    void rejectsInvalidCollection();

    /*! Verifies that duplicate ids are rejected, also across folders. */
    void rejectsDuplicateIds_data();
    /*! Runs one duplicate id case. */
    void rejectsDuplicateIds();

    /*! Verifies the rejection of malformed environments. */
    void rejectsInvalidEnvironments_data();
    /*! Runs one invalid environment case. */
    void rejectsInvalidEnvironments();

    /*! Verifies that an unknown active environment loads as none. */
    void danglingActiveEnvironmentIsCleared();

    /*! Verifies the variable rows returned for the active environment. */
    void activeEnvironmentVariablesReturnsActiveRows();

    /*! Verifies that generated ids are unique UUIDs. */
    void newItemIdIsUniqueUuid();
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
    QVERIFY(result.data.id.isEmpty());
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

void WorkspaceSerializerTest::deepNestingRoundTrips()
{
    CollectionNode node;
    node.kind = CollectionNode::Kind::Request;
    node.id = QStringLiteral("leaf");
    node.name = QStringLiteral("Leaf");
    for (int level = 0; level < 50; ++level) {
        CollectionNode folder;
        folder.id = QStringLiteral("f%1").arg(level);
        folder.name = QStringLiteral("Level %1").arg(level);
        folder.children = {node};
        node = folder;
    }
    WorkspaceData data;
    data.collection = {node};

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("deep.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(path, data));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QVERIFY(result.data == data);
}

void WorkspaceSerializerTest::secretValueIsNeverWritten()
{
    WorkspaceData data = makeWorkspace();
    data.environments[0].variables.append(
        {QStringLiteral("token"), QStringLiteral("s3cret-value"), true, true});

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("secret.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(path, data));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray content = file.readAll();
    QVERIFY(content.contains("token"));
    QVERIFY(!content.contains("s3cret-value"));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY2(result.ok, qPrintable(result.errorString));
    const EnvironmentVariable variable = result.data.environments.at(0).variables.at(2);
    QCOMPARE(variable.key, QStringLiteral("token"));
    QVERIFY(variable.secret);
    QVERIFY(variable.value.isEmpty());
}

void WorkspaceSerializerTest::secretValueInFileIsKept()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("kept.reqdeck"));
    QVERIFY(writeFile(path, R"({"schema": 1, "environments": [{"id": "e1", "name": "E", "variables":
        [{"key": "k", "value": "v", "secret": true}]}]})"));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.data.environments.at(0).variables.at(0).value, QStringLiteral("v"));
}

void WorkspaceSerializerTest::rejectsNonUuidWorkspaceId_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("traversal") << QByteArray(R"({"schema": 1, "id": "../../x"})");
    QTest::newRow("path") << QByteArray(R"({"schema": 1, "id": "a/b"})");
    QTest::newRow("word") << QByteArray(R"({"schema": 1, "id": "abc"})");
    QTest::newRow("number") << QByteArray(R"({"schema": 1, "id": 5})");
}

void WorkspaceSerializerTest::rejectsNonUuidWorkspaceId()
{
    QFETCH(QByteArray, content);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("id.reqdeck"));
    QVERIFY(writeFile(path, content));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::InvalidSchema);
}

void WorkspaceSerializerTest::rejectsInvalidCollection_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("not-object") << QByteArray(R"({"schema": 1, "collection": [1]})");
    QTest::newRow("unknown-kind")
        << QByteArray(R"({"schema": 1, "collection": [{"kind": "x", "id": "a", "name": "n"}]})");
    QTest::newRow("empty-id")
        << QByteArray(R"({"schema": 1, "collection": [{"kind": "folder", "id": "", "name": "n"}]})");
    QTest::newRow("name-number")
        << QByteArray(R"({"schema": 1, "collection": [{"kind": "folder", "id": "a", "name": 1}]})");
    QTest::newRow("children-object") << QByteArray(
        R"({"schema": 1, "collection": [{"kind": "folder", "id": "a", "name": "n", "children": {}}]})");
    QTest::newRow("no-request")
        << QByteArray(R"({"schema": 1, "collection": [{"kind": "request", "id": "a", "name": "n"}]})");
    QTest::newRow("bad-method") << QByteArray(
        R"({"schema": 1, "collection": [{"kind": "request", "id": "a", "name": "n",
            "request": {"method": "FETCH"}}]})");
    QTest::newRow("nested-bad") << QByteArray(
        R"({"schema": 1, "collection": [{"kind": "folder", "id": "a", "name": "n",
            "children": [{"kind": "x", "id": "b", "name": "m"}]}]})");
}

void WorkspaceSerializerTest::rejectsInvalidCollection()
{
    QFETCH(QByteArray, content);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("collection.reqdeck"));
    QVERIFY(writeFile(path, content));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::InvalidSchema);
    QVERIFY(!result.errorString.isEmpty());
}

void WorkspaceSerializerTest::rejectsDuplicateIds_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("siblings") << QByteArray(
        R"({"schema": 1, "collection": [{"kind": "folder", "id": "dup", "name": "a"},
                                        {"kind": "folder", "id": "dup", "name": "b"}]})");
    QTest::newRow("nested") << QByteArray(
        R"({"schema": 1, "collection": [{"kind": "folder", "id": "dup", "name": "a", "children":
            [{"kind": "folder", "id": "dup", "name": "b"}]}]})");
    QTest::newRow("environments") << QByteArray(
        R"({"schema": 1, "environments": [{"id": "dup", "name": "a"}, {"id": "dup", "name": "b"}]})");
}

void WorkspaceSerializerTest::rejectsDuplicateIds()
{
    QFETCH(QByteArray, content);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("dup.reqdeck"));
    QVERIFY(writeFile(path, content));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::InvalidSchema);
    QCOMPARE(result.errorString, QStringLiteral("Duplicate item id: dup"));
}

void WorkspaceSerializerTest::rejectsInvalidEnvironments_data()
{
    QTest::addColumn<QByteArray>("content");
    QTest::newRow("not-object") << QByteArray(R"({"schema": 1, "environments": [1]})");
    QTest::newRow("empty-id") << QByteArray(R"({"schema": 1, "environments": [{"id": "", "name": "n"}]})");
    QTest::newRow("name-number") << QByteArray(R"({"schema": 1, "environments": [{"id": "e", "name": 1}]})");
    QTest::newRow("variables-object")
        << QByteArray(R"({"schema": 1, "environments": [{"id": "e", "name": "n", "variables": {}}]})");
    QTest::newRow("value-number") << QByteArray(
        R"({"schema": 1, "environments": [{"id": "e", "name": "n", "variables": [{"key": "k", "value": 1}]}]})");
    QTest::newRow("secret-string") << QByteArray(
        R"({"schema": 1, "environments": [{"id": "e", "name": "n", "variables": [{"key": "k", "secret": "y"}]}]})");
}

void WorkspaceSerializerTest::rejectsInvalidEnvironments()
{
    QFETCH(QByteArray, content);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("environments.reqdeck"));
    QVERIFY(writeFile(path, content));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(!result.ok);
    QCOMPARE(result.error, WorkspaceSerializer::Error::InvalidSchema);
}

void WorkspaceSerializerTest::danglingActiveEnvironmentIsCleared()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("dangling.reqdeck"));
    QVERIFY(writeFile(path, R"({"schema": 1, "activeEnvironmentId": "gone"})"));

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QVERIFY(result.data.activeEnvironmentId.isEmpty());
}

void WorkspaceSerializerTest::activeEnvironmentVariablesReturnsActiveRows()
{
    WorkspaceData data = makeWorkspace();
    const QList<KeyValue> expected {{QStringLiteral("baseUrl"), QStringLiteral("https://s"), true},
                                    {QStringLiteral("off"), QStringLiteral("x"), false}};
    QVERIFY(activeEnvironmentVariables(data) == expected);

    data.activeEnvironmentId.clear();
    QVERIFY(activeEnvironmentVariables(data).isEmpty());
}

void WorkspaceSerializerTest::newItemIdIsUniqueUuid()
{
    const QString first = newItemId();
    QVERIFY(!QUuid::fromString(first).isNull());
    QVERIFY(first != newItemId());
    QVERIFY(!first.contains(u'{'));
}

QTEST_GUILESS_MAIN(WorkspaceSerializerTest)

#include "tst_workspaceserializer.moc"
