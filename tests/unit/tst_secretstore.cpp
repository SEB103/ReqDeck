// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "core/workspace/secretstore.h"

/*!
 * \internal
 * \brief Returns a workspace with one plain and two secret variables.
 */
static WorkspaceData makeWorkspace()
{
    Environment environment;
    environment.id = QStringLiteral("e1");
    environment.name = QStringLiteral("Staging");
    environment.variables = {{QStringLiteral("baseUrl"), QStringLiteral("plain-value"), true, false},
                             {QStringLiteral("token"), QStringLiteral("secret-value"), true, true},
                             {QStringLiteral("old"), QStringLiteral("off-value"), false, true}};

    WorkspaceData data;
    data.id = QStringLiteral("3f2b8c1e-7a44-4a8e-9a55-0c1d2e3f4a5b");
    data.environments = {environment};
    return data;
}

/*!
 * \internal
 * \brief Returns \a data with the values of all secret variables cleared.
 */
static WorkspaceData withoutSecretValues(WorkspaceData data)
{
    for (Environment &environment : data.environments) {
        for (EnvironmentVariable &variable : environment.variables) {
            if (variable.secret)
                variable.value.clear();
        }
    }
    return data;
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

/*! Verifies the storage of secret variable values outside workspace files. */
class SecretStoreTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that saved values come back, including disabled ones. */
    void saveThenApplyRestoresValues();

    /*! Verifies that the file contains secret values only. */
    void writesOnlySecretValues();

    /*! Verifies that the file disappears when no secret value is left. */
    void noSecretsRemovesFile();

    /*! Verifies that a workspace without a file is not an error. */
    void missingFileIsNotAnError();

    /*! Verifies that a malformed file is reported and set aside. */
    void malformedFileReportsErrorAndKeepsData_data();
    /*! Runs one malformed file case. */
    void malformedFileReportsErrorAndKeepsData();

    /*! Verifies that saving requires a UUID as workspace id. */
    void rejectsInvalidWorkspaceId_data();
    /*! Runs one invalid id case. */
    void rejectsInvalidWorkspaceId();

    /*! Verifies that a stored entry does not touch a variable that is no longer secret. */
    void ignoresEntriesWithoutSecretFlag();

    /*! Verifies that a store directory blocked by a file fails cleanly. */
    void reportsWriteFailure();
};

void SecretStoreTest::saveThenApplyRestoresValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.filePath(QStringLiteral("secrets")));

    const WorkspaceData data = makeWorkspace();
    QString error;
    QVERIFY2(store.save(data, &error), qPrintable(error));

    WorkspaceData loaded = withoutSecretValues(data);
    QVERIFY2(store.applyTo(loaded, &error), qPrintable(error));
    QVERIFY(loaded == data);
}

void SecretStoreTest::writesOnlySecretValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    const WorkspaceData data = makeWorkspace();
    QVERIFY(store.save(data));

    const QByteArray content = readFile(store.filePath(data.id));
    QVERIFY(content.contains("secret-value"));
    QVERIFY(!content.contains("plain-value"));
}

void SecretStoreTest::noSecretsRemovesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    const WorkspaceData data = makeWorkspace();
    QVERIFY(store.save(data));
    QVERIFY(QFile::exists(store.filePath(data.id)));

    QVERIFY(store.save(withoutSecretValues(data)));
    QVERIFY(!QFile::exists(store.filePath(data.id)));
}

void SecretStoreTest::missingFileIsNotAnError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    WorkspaceData data = withoutSecretValues(makeWorkspace());
    const WorkspaceData before = data;
    QVERIFY(store.applyTo(data));
    QVERIFY(data == before);
}

void SecretStoreTest::malformedFileReportsErrorAndKeepsData_data()
{
    QTest::addColumn<QByteArray>("content");

    QTest::newRow("not-json") << QByteArray("{oops");
    QTest::newRow("array-root") << QByteArray("[]");
    QTest::newRow("environments-string") << QByteArray("{\"schema\":1,\"environments\":\"x\"}");
    QTest::newRow("future-schema") << QByteArray("{\"schema\":2,\"environments\":{}}");
}

void SecretStoreTest::malformedFileReportsErrorAndKeepsData()
{
    QFETCH(QByteArray, content);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    WorkspaceData data = withoutSecretValues(makeWorkspace());
    const QString path = store.filePath(data.id);
    QVERIFY(writeFile(path, content));

    const WorkspaceData before = data;
    QString error;
    QVERIFY(!store.applyTo(data, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(data == before);
    QVERIFY(!QFile::exists(path));
    QCOMPARE(readFile(path + QStringLiteral(".corrupt")), content);
}

void SecretStoreTest::rejectsInvalidWorkspaceId_data()
{
    QTest::addColumn<QString>("id");

    QTest::newRow("empty") << QString();
    QTest::newRow("traversal") << QStringLiteral("../../x");
}

void SecretStoreTest::rejectsInvalidWorkspaceId()
{
    QFETCH(QString, id);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    WorkspaceData data = makeWorkspace();
    data.id = id;
    QString error;
    QVERIFY(!store.save(data, &error));
    QVERIFY(!error.isEmpty());
}

void SecretStoreTest::ignoresEntriesWithoutSecretFlag()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    WorkspaceData data = makeWorkspace();
    QVERIFY(store.save(data));

    data.environments[0].variables[1].secret = false;
    data.environments[0].variables[1].value = QStringLiteral("now-plain");
    QVERIFY(store.applyTo(data));
    QCOMPARE(data.environments.at(0).variables.at(1).value, QStringLiteral("now-plain"));
}

void SecretStoreTest::reportsWriteFailure()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString blocked = dir.filePath(QStringLiteral("blocked"));
    QVERIFY(writeFile(blocked, "file"));

    QString error;
    QVERIFY(!SecretStore(blocked).save(makeWorkspace(), &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(SecretStoreTest)

#include "tst_secretstore.moc"
