// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "core/workspace/secretcipher.h"
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

    /*! Verifies that the file holds no value in plain text, secret or not. */
    void fileHoldsNoPlainValue();

    /*! Verifies that a file of schema 1, with plain values, is still read. */
    void readsSchemaOne();

    /*! Verifies that a file encrypted with another key is unavailable and stays as it is. */
    void otherKeyIsUnavailable();

    /*! Verifies that the cipher turns data back and refuses another key. */
    void cipherRoundTrip();

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

    /*! Verifies that fillEmptyValues() keeps a value entered in the meantime. */
    void fillEmptyValuesKeepsEnteredValues();

    /*! Verifies that a file that can neither be read nor moved is reported as such. */
    void unmovableFileIsUnreadable();

    /*! Verifies that a successful set-aside replaces an older copy. */
    void setAsideReplacesOlderCopy();

    /*! Verifies that a snapshot puts the file back, or removes it when there was none. */
    void restoreBringsBackSnapshot();

    /*! Verifies that rows sharing a key keep their own values. */
    void duplicateSecretKeysKeepTheirValues();
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
    QCOMPARE(store.applyTo(loaded), SecretStore::ReadStatus::Ok);
    QVERIFY(loaded == data);
}

void SecretStoreTest::fileHoldsNoPlainValue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    const WorkspaceData data = makeWorkspace();
    QVERIFY(store.save(data));

    const QByteArray content = readFile(store.filePath(data.id));
    QVERIFY(!content.isEmpty());
    QVERIFY(!content.contains("secret-value"));
    QVERIFY(!content.contains("off-value"));
    QVERIFY(!content.contains("token"));
    QVERIFY(!content.contains("plain-value"));
    QVERIFY(content.contains("\"schema\": 2"));
}

void SecretStoreTest::readsSchemaOne()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());
    const WorkspaceData data = makeWorkspace();
    QVERIFY(writeFile(store.filePath(data.id),
                      "{\"schema\":1,\"environments\":{\"e1\":{\"token\":[\"secret-value\"],"
                      "\"old\":[\"off-value\"]}}}"));

    WorkspaceData loaded = withoutSecretValues(data);
    QCOMPARE(store.applyTo(loaded), SecretStore::ReadStatus::Ok);
    QVERIFY(loaded == data);
}

void SecretStoreTest::otherKeyIsUnavailable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore other(dir.path(), SecretCipher(QByteArrayLiteral("another computer")));
    const WorkspaceData data = makeWorkspace();
    QVERIFY(other.save(data));
    const QString path = other.filePath(data.id);
    const QByteArray content = readFile(path);

    const SecretStore store(dir.path());
    WorkspaceData loaded = withoutSecretValues(data);
    const WorkspaceData before = loaded;
    QCOMPARE(store.applyTo(loaded), SecretStore::ReadStatus::Unavailable);
    QVERIFY(loaded == before);
    QCOMPARE(readFile(path), content);
    QVERIFY(!QFile::exists(path + QStringLiteral(".corrupt")));
    QCOMPARE(store.fillEmptyValues(loaded), SecretStore::ReadStatus::Unavailable);
}

void SecretStoreTest::cipherRoundTrip()
{
    const SecretCipher cipher;
    const QByteArray plain("value with \0 inside", 19);

    const std::optional<QByteArray> sealed = cipher.protect(plain);
    QVERIFY(sealed);
    QVERIFY(!sealed->contains("value"));
    QCOMPARE(cipher.unprotect(*sealed), std::optional<QByteArray>(plain));
    QVERIFY(!SecretCipher(QByteArrayLiteral("other")).unprotect(*sealed));
    QVERIFY(!cipher.unprotect(QByteArrayLiteral("not sealed")));
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
    QCOMPARE(store.applyTo(data), SecretStore::ReadStatus::Ok);
    QVERIFY(data == before);
}

void SecretStoreTest::malformedFileReportsErrorAndKeepsData_data()
{
    QTest::addColumn<QByteArray>("content");

    QTest::newRow("not-json") << QByteArray("{oops");
    QTest::newRow("array-root") << QByteArray("[]");
    QTest::newRow("environments-string") << QByteArray("{\"schema\":1,\"environments\":\"x\"}");
    QTest::newRow("future-schema") << QByteArray("{\"schema\":3,\"data\":\"AAAA\"}");
    QTest::newRow("encrypted-without-data") << QByteArray("{\"schema\":2,\"protection\":\"dpapi\"}");
    QTest::newRow("encrypted-data-number") << QByteArray("{\"schema\":2,\"protection\":\"dpapi\",\"data\":5}");
    QTest::newRow("unknown-protection") << QByteArray("{\"schema\":2,\"protection\":\"rot13\",\"data\":\"AAAA\"}");
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
    QCOMPARE(store.applyTo(data), SecretStore::ReadStatus::Corrupt);
    QVERIFY(data == before);
    QVERIFY(!QFile::exists(path));
    QCOMPARE(readFile(path + QStringLiteral(".corrupt")), content);
}

void SecretStoreTest::rejectsInvalidWorkspaceId_data()
{
    QTest::addColumn<QString>("id");

    QTest::newRow("empty") << QString();
    QTest::newRow("traversal") << QStringLiteral("../../x");
    QTest::newRow("prefix-traversal") << QStringLiteral("12345678-1234-1234-1234-123456789abc/../victim");
    QTest::newRow("prefix-backslash") << QStringLiteral("12345678-1234-1234-1234-123456789abc\\..\\victim");
    QTest::newRow("braced") << QStringLiteral("{12345678-1234-1234-1234-123456789abc}");
}

void SecretStoreTest::rejectsInvalidWorkspaceId()
{
    QFETCH(QString, id);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    const QString victim = dir.filePath(QStringLiteral("victim.json"));
    QVERIFY(writeFile(victim, "{oops"));

    WorkspaceData data = makeWorkspace();
    data.id = id;
    QString error;
    QVERIFY(!store.save(data, &error));
    QVERIFY(!error.isEmpty());

    store.applyTo(data);
    QCOMPARE(readFile(victim), QByteArray("{oops"));
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
    QCOMPARE(store.applyTo(data), SecretStore::ReadStatus::Ok);
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

void SecretStoreTest::fillEmptyValuesKeepsEnteredValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    const WorkspaceData data = makeWorkspace();
    QVERIFY(store.save(data));

    WorkspaceData loaded = withoutSecretValues(data);
    loaded.environments[0].variables[1].value = QStringLiteral("entered");
    QCOMPARE(store.fillEmptyValues(loaded), SecretStore::ReadStatus::Ok);
    QCOMPARE(loaded.environments.at(0).variables.at(1).value, QStringLiteral("entered"));
    QCOMPARE(loaded.environments.at(0).variables.at(2).value, QStringLiteral("off-value"));
}

void SecretStoreTest::unmovableFileIsUnreadable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    WorkspaceData data = withoutSecretValues(makeWorkspace());
    const QString path = store.filePath(data.id);
    QVERIFY(writeFile(path, "{oops"));
    QVERIFY(writeFile(path + QStringLiteral(".corrupt"), "older"));
    QVERIFY(QDir().mkpath(path + QStringLiteral(".corrupt.new/keep")));

    QCOMPARE(store.applyTo(data), SecretStore::ReadStatus::Unreadable);
    QCOMPARE(readFile(path), QByteArray("{oops"));
    QCOMPARE(readFile(path + QStringLiteral(".corrupt")), QByteArray("older"));
}

void SecretStoreTest::setAsideReplacesOlderCopy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    WorkspaceData data = withoutSecretValues(makeWorkspace());
    const QString path = store.filePath(data.id);
    QVERIFY(writeFile(path, "{new"));
    QVERIFY(writeFile(path + QStringLiteral(".corrupt"), "older"));

    QCOMPARE(store.applyTo(data), SecretStore::ReadStatus::Corrupt);
    QCOMPARE(readFile(path + QStringLiteral(".corrupt")), QByteArray("{new"));
    QVERIFY(!QFile::exists(path + QStringLiteral(".corrupt.new")));
}

void SecretStoreTest::restoreBringsBackSnapshot()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    const WorkspaceData data = makeWorkspace();
    QVERIFY(!store.snapshot(data.id));

    QVERIFY(store.save(data));
    const std::optional<QByteArray> saved = store.snapshot(data.id);
    QVERIFY(saved);

    QVERIFY(store.save(withoutSecretValues(data)));
    QVERIFY(store.restore(data.id, saved));
    QCOMPARE(readFile(store.filePath(data.id)), *saved);

    QVERIFY(store.restore(data.id, std::nullopt));
    QVERIFY(!QFile::exists(store.filePath(data.id)));
}

void SecretStoreTest::duplicateSecretKeysKeepTheirValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const SecretStore store(dir.path());

    WorkspaceData data = makeWorkspace();
    data.environments[0].variables = {
        {QStringLiteral("token"), QStringLiteral("staging"), false, true},
        {QStringLiteral("token"), QStringLiteral("production"), true, true},
    };
    QVERIFY(store.save(data));

    WorkspaceData loaded = withoutSecretValues(data);
    store.applyTo(loaded);
    QVERIFY(loaded == data);
}

QTEST_GUILESS_MAIN(SecretStoreTest)

#include "tst_secretstore.moc"
