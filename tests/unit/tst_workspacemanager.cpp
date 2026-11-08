// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>
#include <QtTest>

#include "core/workspace/secretcipher.h"
#include "core/workspace/secretstore.h"
#include "core/workspace/workspacebackup.h"
#include "core/workspace/workspaceserializer.h"
#include "productinfo.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \internal
 * \brief Test-only subclass exposing the protected recent-list and active-state API.
 */
class TestableWorkspaceManager : public WorkspaceManager
{
public:
    using WorkspaceManager::addOrUpdateRecent;
    using WorkspaceManager::clearActiveWorkspace;
    using WorkspaceManager::setActiveWorkspace;
    using WorkspaceManager::setDirty;
};

/*!
 * \internal
 * \brief Returns \a data with one environment holding a plain and a secret variable.
 */
static WorkspaceData withSecret(WorkspaceData data, const QString &secretValue)
{
    Environment environment;
    environment.id = QStringLiteral("e1");
    environment.name = QStringLiteral("Env");
    environment.variables = {{QStringLiteral("baseUrl"), QStringLiteral("https://h"), true, false},
                             {QStringLiteral("token"), secretValue, true, true}};
    data.environments = {environment};
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
 * \brief Returns the decrypted values of the secrets file at \a path, as JSON text.
 */
static QByteArray storedSecrets(const QString &path)
{
    const QJsonObject root = QJsonDocument::fromJson(readFile(path)).object();
    const QByteArray sealed = QByteArray::fromBase64(root.value(QLatin1StringView("data")).toString().toLatin1());
    return SecretCipher().unprotect(sealed).value_or(QByteArray());
}

/*! Verifies WorkspaceManager recent-list, file, and active-workspace behavior. */
class WorkspaceManagerTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Keeps per-user default locations away from the real user profile. */
    void initTestCase();

    /*! Verifies newest-first ordering and case-insensitive de-duplication. */
    void recentListOrdersNewestFirstAndDeduplicates();

    /*! Verifies that the recent list is capped at ten entries. */
    void recentListIsCapped();

    /*! Verifies that a missing file is reported as unavailable. */
    void recentRowReportsAvailability();

    /*! Verifies that the recent list survives a settings reload. */
    void recentListPersistsAcrossReload();

    /*! Verifies that removing a recent entry updates the list. */
    void removeRecentDropsEntry();

    /*! Verifies active-workspace state and dirty transitions. */
    void activeWorkspaceStateTracksSelection();

    /*! Verifies that creating a workspace writes a valid file and activates it. */
    void createWorkspaceWritesFileAndActivates();

    /*! Verifies that the .reqdeck suffix is appended and file: URLs are accepted. */
    void createWorkspaceAppendsSuffixAndAcceptsUrls();

    /*! Verifies that creating a workspace with an empty name is refused. */
    void createWorkspaceRejectsEmptyName();

    /*! Verifies that creating a workspace over an existing file is refused. */
    void createRefusesToOverwriteExistingFile();

    /*! Verifies that opening an existing workspace loads and activates it. */
    void openWorkspaceLoadsAndActivates();

    /*! Verifies that a workspace without a stored name is shown under its file name. */
    void openWorkspaceFallsBackToFileName();

    /*! Verifies that opening a missing workspace reports an error and stays inactive. */
    void openMissingWorkspaceReportsError();

    /*! Verifies that a data change marks dirty and saving clears it and persists. */
    void saveClearsDirtyAndPersistsChanges();

    /*! Verifies that unchanged data or no active workspace leaves the dirty flag alone. */
    void updateWithoutChangeKeepsClean();

    /*! Verifies that Save As writes a new file and makes it the active workspace. */
    void saveAsSwitchesActiveFile();

    /*! Verifies that saving without an active workspace reports an error. */
    void saveWithoutActiveWorkspaceReportsError();

    /*! Verifies that closing discards the in-memory data and reports it. */
    void closeWorkspaceDiscardsData();

    /*! Verifies the default workspaces directory falls back to Documents/<identifier>. */
    void defaultWorkspacesDirFallsBackToDocuments();

    /*! Verifies that setting the default workspaces directory persists across reload. */
    void defaultWorkspacesDirPersists();

    /*! Verifies that a new workspace gets a UUID as id. */
    void createAssignsWorkspaceId();

    /*! Verifies that opening a file without id assigns one and stays clean. */
    void openAssignsMissingIdWithoutDirty();

    /*! Verifies that secret values live outside the file and survive reopening. */
    void secretsSurviveReopen();

    /*! Verifies that a failing secrets write leaves the workspace file untouched. */
    void secretsFailureLeavesFileUntouched();

    /*! Verifies that a corrupt secrets file warns but opens the workspace. */
    void corruptSecretsKeepWorkspaceOpen();

    /*! Verifies that clearing the secret flag moves the value into the file. */
    void clearingSecretFlagMovesValueToFile();

    /*! Verifies that removing the last secret removes its store file. */
    void removedSecretLeavesStore();

    /*! Verifies that Save As gives the copy its own id and secrets. */
    void saveAsGetsOwnSecrets();

    /*! Verifies that saving to the same file keeps the workspace id. */
    void saveKeepsWorkspaceId();

    /*! Verifies that a data update cannot replace the workspace id. */
    void updateKeepsWorkspaceId();

    /*! Verifies that a save is refused while stored secrets cannot be read. */
    void unreadableSecretsBlockSave();

    /*! Verifies that a later save recovers secrets that could not be read on open. */
    void unreadableSecretsAreRecoveredOnSave();

    /*! Verifies that the seeded samples open like any workspace. */
    void openSamplesOpensSeededWorkspace();

    /*! Verifies that missing samples are reported, not ignored. */
    void openSamplesMissingFileReportsError();

    /*! Verifies that retrying changes nothing while the secrets stay unreadable. */
    void retrySecretsKeepsStateWhileLocked();

    /*! Verifies that retrying fills in the values without counting as a change. */
    void retrySecretsFillsValuesOnceReadable();

    /*! Verifies that recovery keeps a value the user entered in the meantime. */
    void recoveryKeepsReenteredValue();

    /*! Verifies that a corrupt file that cannot be moved aside is not overwritten. */
    void setAsideFailureBlocksSave();

    /*! Verifies that a failed workspace write restores the previous secrets. */
    void workspaceWriteFailureRestoresSecrets();

    /*! Verifies that a failed Save As leaves no secrets file behind. */
    void failedSaveAsLeavesNoSecrets();

    /*! Verifies that a plain secrets file is encrypted as soon as the workspace opens. */
    void migratesPlainFileOnOpen();

    /*! Verifies that secrets of another computer are reported and do not block saving. */
    void unavailableSecretsDoNotBlockSaving();

    /*! Verifies that the file of another computer stays until a value is saved here. */
    void unavailableFileStaysUntilValues();

    /*! Verifies that saving without a change leaves both files as they were. */
    void unchangedSaveKeepsFiles();

    /*! Verifies that encrypting a plain secrets file first keeps a copy of both files. */
    void backupBeforeFormatChange();

    /*! Verifies that the copy holds no secret value in plain text. */
    void backupKeepsSecretsEncrypted();

    /*! Verifies that opening and saving in the current format makes no copy. */
    void noBackupWithoutFormatChange();

    /*! Verifies that Restore puts back the copied files and reopens the workspace. */
    void restoreBringsBackPreviousCopy();

    /*! Verifies that Restore without a copy reports an error and changes nothing. */
    void restoreWithoutCopyFails();

    /*! Verifies that the backup store refuses an id that is not a UUID. */
    void backupRejectsInvalidId();

    /*! Verifies that a secret value kept in the workspace file survives a save without changes. */
    void inlineSecretSurvivesUnchangedSave();

    /*! Verifies that the copy before encryption keeps values no variable uses any more. */
    void migrationCopyKeepsEveryValue();

    /*! Verifies that a file of another computer is copied before the first value replaces it. */
    void unavailableFileCopiedBeforeReplaced();

private:
    /*! Builds a settings store inside \a dir for one test. */
    static QSettings *makeSettings(QTemporaryDir &dir, QObject *parent);

    /*! Creates a workspace in \a dir whose secrets file cannot be read; returns that file. */
    static QString makeLockedWorkspace(QTemporaryDir &dir, QByteArray &secrets);

    /*! Creates a workspace in \a dir whose secrets were encrypted on another computer; returns that file. */
    static QString makeForeignWorkspace(QTemporaryDir &dir);

    /*! Creates \a dir/Plain.reqdeck whose secrets file is plain schema 1 with "plain-one"; returns that file. */
    static QString makePlainWorkspace(QTemporaryDir &dir);
};

QSettings *WorkspaceManagerTest::makeSettings(QTemporaryDir &dir, QObject *parent)
{
    return new QSettings(dir.filePath(QStringLiteral("settings.ini")),
                         QSettings::IniFormat,
                         parent);
}

void WorkspaceManagerTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral(PRODUCT_IDENTIFIER));
    QCoreApplication::setApplicationName(QStringLiteral(PRODUCT_IDENTIFIER));
    QStandardPaths::setTestModeEnabled(true);
}

void WorkspaceManagerTest::recentListOrdersNewestFirstAndDeduplicates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    manager.addOrUpdateRecent(dir.filePath(QStringLiteral("a.reqdeck")), QStringLiteral("A"));
    manager.addOrUpdateRecent(dir.filePath(QStringLiteral("b.reqdeck")), QStringLiteral("B"));

    QVariantList rows = manager.recentWorkspaces();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("B"));

    // Re-adding "a" (with different casing) moves it to the front without duplicating.
    manager.addOrUpdateRecent(dir.filePath(QStringLiteral("A.REQDECK")), QStringLiteral("A2"));
    rows = manager.recentWorkspaces();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("A2"));
    QVERIFY(!rows.at(0).toMap().value(QStringLiteral("lastOpened")).toString().isEmpty());
}

void WorkspaceManagerTest::recentListIsCapped()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    for (int i = 0; i < 15; ++i) {
        manager.addOrUpdateRecent(dir.filePath(QStringLiteral("w%1.reqdeck").arg(i)),
                                  QStringLiteral("W%1").arg(i));
    }

    const QVariantList rows = manager.recentWorkspaces();
    QCOMPARE(rows.size(), 10);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("W14"));
}

void WorkspaceManagerTest::recentRowReportsAvailability()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    const QString existingPath = dir.filePath(QStringLiteral("real.reqdeck"));
    QFile file(existingPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{}");
    file.close();

    manager.addOrUpdateRecent(existingPath, QStringLiteral("Real"));
    manager.addOrUpdateRecent(dir.filePath(QStringLiteral("ghost.reqdeck")),
                              QStringLiteral("Ghost"));

    const QVariantList rows = manager.recentWorkspaces();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Ghost"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("available")).toBool(), false);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("available")).toBool(), true);
}

void WorkspaceManagerTest::recentListPersistsAcrossReload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString settingsPath = dir.filePath(QStringLiteral("settings.ini"));

    {
        TestableWorkspaceManager manager;
        manager.setSettings(new QSettings(settingsPath, QSettings::IniFormat, &manager));
        manager.addOrUpdateRecent(dir.filePath(QStringLiteral("keep.reqdeck")),
                                  QStringLiteral("Keep"));
    }

    TestableWorkspaceManager reloaded;
    reloaded.setSettings(new QSettings(settingsPath, QSettings::IniFormat, &reloaded));

    const QVariantList rows = reloaded.recentWorkspaces();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Keep"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("path")).toString(),
             QDir::cleanPath(dir.filePath(QStringLiteral("keep.reqdeck"))));
}

void WorkspaceManagerTest::removeRecentDropsEntry()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.addOrUpdateRecent(dir.filePath(QStringLiteral("a.reqdeck")), QStringLiteral("A"));
    manager.addOrUpdateRecent(dir.filePath(QStringLiteral("b.reqdeck")), QStringLiteral("B"));

    QSignalSpy spy(&manager, &WorkspaceManager::recentWorkspacesChanged);
    manager.removeRecent(0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(manager.recentWorkspaces().size(), 1);
    QCOMPARE(manager.recentWorkspaces().at(0).toMap().value(QStringLiteral("displayName")),
             QVariant(QStringLiteral("A")));

    manager.removeRecent(5);
    QCOMPARE(spy.count(), 1);
}

void WorkspaceManagerTest::activeWorkspaceStateTracksSelection()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    QVERIFY(!manager.hasActiveWorkspace());

    QSignalSpy activeSpy(&manager, &WorkspaceManager::activeWorkspaceChanged);
    QSignalSpy dirtySpy(&manager, &WorkspaceManager::dirtyChanged);

    WorkspaceData data;
    data.displayName = QStringLiteral("X");
    manager.setActiveWorkspace(dir.filePath(QStringLiteral("x.reqdeck")), data);
    QVERIFY(manager.hasActiveWorkspace());
    QCOMPARE(manager.activeWorkspaceName(), QStringLiteral("X"));
    QCOMPARE(activeSpy.count(), 1);
    QVERIFY(!manager.dirty());

    manager.setDirty(true);
    QVERIFY(manager.dirty());
    QCOMPARE(dirtySpy.count(), 1);

    manager.clearActiveWorkspace();
    QVERIFY(!manager.hasActiveWorkspace());
    QCOMPARE(manager.activeWorkspaceName(), QString());
    QCOMPARE(manager.activeWorkspacePath(), QString());
    QVERIFY(!manager.dirty());
}

void WorkspaceManagerTest::createWorkspaceWritesFileAndActivates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    QSignalSpy errorSpy(&manager, &WorkspaceManager::workspaceError);
    QSignalSpy notifySpy(&manager, &WorkspaceManager::notification);

    const QString path = dir.filePath(QStringLiteral("New.reqdeck"));
    QVERIFY(manager.createWorkspaceAtPath(path));
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(notifySpy.count(), 1);

    QVERIFY(QFileInfo::exists(path));
    QVERIFY(manager.hasActiveWorkspace());
    QCOMPARE(manager.activeWorkspaceName(), QStringLiteral("New"));
    QCOMPARE(manager.activeWorkspacePath(), QDir::cleanPath(path));
    QCOMPARE(manager.recentWorkspaces().size(), 1);

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(result.ok);
    QCOMPARE(result.data.displayName, QStringLiteral("New"));
}

void WorkspaceManagerTest::createWorkspaceAppendsSuffixAndAcceptsUrls()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    QVERIFY(manager.createWorkspace(QStringLiteral("  Payments  "),
                                    QUrl::fromLocalFile(dir.path()).toString()));
    const QString expected = QDir::cleanPath(dir.filePath(QStringLiteral("Payments.reqdeck")));
    QVERIFY(QFileInfo::exists(expected));
    QCOMPARE(manager.activeWorkspacePath(), expected);
    QCOMPARE(manager.activeWorkspaceName(), QStringLiteral("Payments"));
}

void WorkspaceManagerTest::createWorkspaceRejectsEmptyName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    QSignalSpy errorSpy(&manager, &WorkspaceManager::workspaceError);

    QVERIFY(!manager.createWorkspace(QStringLiteral("   "), dir.path()));
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!manager.hasActiveWorkspace());
}

void WorkspaceManagerTest::createRefusesToOverwriteExistingFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    const QString path = dir.filePath(QStringLiteral("Existing.reqdeck"));
    QVERIFY(manager.createWorkspaceAtPath(path));

    manager.clearActiveWorkspace();
    QSignalSpy errorSpy(&manager, &WorkspaceManager::workspaceError);
    QVERIFY(!manager.createWorkspaceAtPath(path));
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!manager.hasActiveWorkspace());
}

void WorkspaceManagerTest::openWorkspaceLoadsAndActivates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("Loaded.reqdeck"));
    WorkspaceData data;
    data.displayName = QStringLiteral("Loaded Workspace");
    data.environments = {{QStringLiteral("env-1"), QStringLiteral("Env"), {}}};
    data.activeEnvironmentId = QStringLiteral("env-1");
    QString error;
    QVERIFY2(WorkspaceSerializer::save(path, data, &error), qPrintable(error));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    QVERIFY(manager.openWorkspace(path));
    QVERIFY(manager.hasActiveWorkspace());
    QVERIFY(!manager.dirty());
    QCOMPARE(manager.activeWorkspaceName(), QStringLiteral("Loaded Workspace"));
    QCOMPARE(manager.workspaceData().activeEnvironmentId, QStringLiteral("env-1"));
    QCOMPARE(manager.recentWorkspaces().size(), 1);

    // Opening through the recent list reloads the same file.
    manager.closeWorkspace();
    QVERIFY(manager.openRecent(0));
    QCOMPARE(manager.activeWorkspaceName(), QStringLiteral("Loaded Workspace"));
    QVERIFY(!manager.openRecent(3));
}

void WorkspaceManagerTest::openWorkspaceFallsBackToFileName()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("Unnamed.reqdeck"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"schema": 1})");
    file.close();

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    QVERIFY(manager.openWorkspace(path));
    QCOMPARE(manager.activeWorkspaceName(), QStringLiteral("Unnamed"));
}

void WorkspaceManagerTest::openMissingWorkspaceReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    QSignalSpy errorSpy(&manager, &WorkspaceManager::workspaceError);

    QVERIFY(!manager.openWorkspace(dir.filePath(QStringLiteral("ghost.reqdeck"))));
    QVERIFY(!manager.hasActiveWorkspace());
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(manager.recentWorkspaces().isEmpty());
}

void WorkspaceManagerTest::saveClearsDirtyAndPersistsChanges()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    const QString path = dir.filePath(QStringLiteral("Dirty.reqdeck"));
    QVERIFY(manager.createWorkspaceAtPath(path));
    QVERIFY(!manager.dirty());

    WorkspaceData changed = manager.workspaceData();
    changed.settings.insert(QStringLiteral("timeoutMs"), 45000);
    manager.updateWorkspaceData(changed);
    QVERIFY(manager.dirty());

    QVERIFY(manager.saveWorkspace());
    QVERIFY(!manager.dirty());

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(path);
    QVERIFY(result.ok);
    QCOMPARE(result.data.settings.value(QStringLiteral("timeoutMs")).toInt(), 45000);
}

void WorkspaceManagerTest::updateWithoutChangeKeepsClean()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    // Without an active workspace the update is ignored.
    WorkspaceData data;
    data.displayName = QStringLiteral("Orphan");
    manager.updateWorkspaceData(data);
    QVERIFY(!manager.dirty());
    QVERIFY(manager.workspaceData().displayName.isEmpty());

    QVERIFY(manager.createWorkspaceAtPath(dir.filePath(QStringLiteral("Clean.reqdeck"))));
    QSignalSpy dirtySpy(&manager, &WorkspaceManager::dirtyChanged);
    manager.updateWorkspaceData(manager.workspaceData());
    QVERIFY(!manager.dirty());
    QCOMPARE(dirtySpy.count(), 0);
}

void WorkspaceManagerTest::saveAsSwitchesActiveFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    QVERIFY(manager.createWorkspaceAtPath(dir.filePath(QStringLiteral("First.reqdeck"))));
    WorkspaceData changed = manager.workspaceData();
    changed.environments = {{QStringLiteral("env-2"), QStringLiteral("Env"), {}}};
    changed.activeEnvironmentId = QStringLiteral("env-2");
    manager.updateWorkspaceData(changed);

    const QString copyPath = dir.filePath(QStringLiteral("Copy"));
    QVERIFY(manager.saveWorkspaceAs(copyPath));
    const QString expected = QDir::cleanPath(copyPath + QStringLiteral(".reqdeck"));
    QCOMPARE(manager.activeWorkspacePath(), expected);
    QCOMPARE(manager.activeWorkspaceName(), QStringLiteral("Copy"));
    QVERIFY(!manager.dirty());
    QCOMPARE(manager.recentWorkspaces().size(), 2);

    const WorkspaceSerializer::LoadResult result = WorkspaceSerializer::load(expected);
    QVERIFY(result.ok);
    QCOMPARE(result.data.displayName, QStringLiteral("Copy"));
    QCOMPARE(result.data.activeEnvironmentId, QStringLiteral("env-2"));
}

void WorkspaceManagerTest::saveWithoutActiveWorkspaceReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    QSignalSpy errorSpy(&manager, &WorkspaceManager::workspaceError);

    QVERIFY(!manager.saveWorkspace());
    QVERIFY(!manager.saveWorkspaceAs(dir.filePath(QStringLiteral("x.reqdeck"))));
    QCOMPARE(errorSpy.count(), 2);
}

void WorkspaceManagerTest::closeWorkspaceDiscardsData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    QVERIFY(manager.createWorkspaceAtPath(dir.filePath(QStringLiteral("Close.reqdeck"))));

    WorkspaceData changed = manager.workspaceData();
    changed.activeEnvironmentId = QStringLiteral("env-3");
    manager.updateWorkspaceData(changed);
    QVERIFY(manager.dirty());

    QSignalSpy notifySpy(&manager, &WorkspaceManager::notification);
    manager.closeWorkspace();
    QVERIFY(!manager.hasActiveWorkspace());
    QVERIFY(!manager.dirty());
    QVERIFY(manager.workspaceData() == WorkspaceData());
    QCOMPARE(notifySpy.count(), 1);

    // Closing again without an active workspace does not notify.
    manager.closeWorkspace();
    QCOMPARE(notifySpy.count(), 1);
}

void WorkspaceManagerTest::defaultWorkspacesDirFallsBackToDocuments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));

    const QString fallback = manager.defaultWorkspacesDir();
    QVERIFY(!fallback.isEmpty());
    QVERIFY(fallback.endsWith(QStringLiteral(PRODUCT_IDENTIFIER)));
}

void WorkspaceManagerTest::defaultWorkspacesDirPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString settingsPath = dir.filePath(QStringLiteral("settings.ini"));
    const QString chosen = dir.filePath(QStringLiteral("MyWorkspaces"));

    {
        TestableWorkspaceManager manager;
        manager.setSettings(new QSettings(settingsPath, QSettings::IniFormat, &manager));

        QSignalSpy spy(&manager, &WorkspaceManager::defaultWorkspacesDirChanged);
        manager.setDefaultWorkspacesDir(chosen);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(manager.defaultWorkspacesDir(), QDir::cleanPath(chosen));
        QVERIFY(QFileInfo(chosen).isDir());
    }

    TestableWorkspaceManager reloaded;
    reloaded.setSettings(new QSettings(settingsPath, QSettings::IniFormat, &reloaded));
    QCOMPARE(reloaded.defaultWorkspacesDir(), QDir::cleanPath(chosen));
}

void WorkspaceManagerTest::createAssignsWorkspaceId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    const QString path = dir.filePath(QStringLiteral("Id.reqdeck"));
    QVERIFY(manager.createWorkspaceAtPath(path));

    QVERIFY(!QUuid::fromString(manager.workspaceData().id).isNull());
    QCOMPARE(WorkspaceSerializer::load(path).data.id, manager.workspaceData().id);
}

void WorkspaceManagerTest::openAssignsMissingIdWithoutDirty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("Old.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(path, WorkspaceData {}));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(path));

    QVERIFY(!QUuid::fromString(manager.workspaceData().id).isNull());
    QVERIFY(!manager.dirty());

    QVERIFY(manager.saveWorkspace());
    QCOMPARE(WorkspaceSerializer::load(path).data.id, manager.workspaceData().id);
}

void WorkspaceManagerTest::secretsSurviveReopen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Secret.reqdeck"));

    {
        TestableWorkspaceManager manager;
        manager.setSettings(makeSettings(dir, &manager));
        manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
        QVERIFY(manager.createWorkspaceAtPath(path));
        manager.updateWorkspaceData(
            withSecret(manager.workspaceData(), QStringLiteral("s3cret-value")));
        QVERIFY(manager.saveWorkspace());
    }

    QVERIFY(!readFile(path).contains("s3cret-value"));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(path));
    QCOMPARE(manager.workspaceData().environments.at(0).variables.at(1).value,
             QStringLiteral("s3cret-value"));
    QVERIFY(!manager.dirty());
}

void WorkspaceManagerTest::secretsFailureLeavesFileUntouched()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Blocked.reqdeck"));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.createWorkspaceAtPath(path));
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("v")));
    const QByteArray before = readFile(path);

    const QString blocked = dir.filePath(QStringLiteral("blocked"));
    QFile blocker(blocked);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    manager.setSecretsDir(blocked);

    QSignalSpy errorSpy(&manager, &WorkspaceManager::workspaceError);
    QVERIFY(!manager.saveWorkspace());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(readFile(path), before);
    QVERIFY(manager.dirty());
}

void WorkspaceManagerTest::corruptSecretsKeepWorkspaceOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Corrupt.reqdeck"));
    const QString secretsDir = dir.filePath(QStringLiteral("secrets"));

    QString id;
    {
        TestableWorkspaceManager manager;
        manager.setSettings(makeSettings(dir, &manager));
        manager.setSecretsDir(secretsDir);
        QVERIFY(manager.createWorkspaceAtPath(path));
        manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("v")));
        QVERIFY(manager.saveWorkspace());
        id = manager.workspaceData().id;
    }

    QFile secrets(SecretStore(secretsDir).filePath(id));
    QVERIFY(secrets.open(QIODevice::WriteOnly));
    secrets.write("{oops");
    secrets.close();

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(secretsDir);
    QSignalSpy notifySpy(&manager, &WorkspaceManager::notification);
    QVERIFY(manager.openWorkspace(path));

    int warnings = 0;
    for (const QList<QVariant> &arguments : std::as_const(notifySpy)) {
        if (arguments.at(0).toInt() == Diagnostics::Warning)
            ++warnings;
    }
    QCOMPARE(warnings, 1);
    QVERIFY(manager.workspaceData().environments.at(0).variables.at(1).value.isEmpty());
}

void WorkspaceManagerTest::clearingSecretFlagMovesValueToFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Flag.reqdeck"));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.createWorkspaceAtPath(path));
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("now-public")));
    QVERIFY(manager.saveWorkspace());
    QVERIFY(!readFile(path).contains("now-public"));

    WorkspaceData changed = manager.workspaceData();
    changed.environments[0].variables[1].secret = false;
    manager.updateWorkspaceData(changed);
    QVERIFY(manager.saveWorkspace());

    QVERIFY(readFile(path).contains("now-public"));
    QVERIFY(!QFile::exists(SecretStore(manager.secretsDir()).filePath(changed.id)));
}

void WorkspaceManagerTest::removedSecretLeavesStore()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.createWorkspaceAtPath(dir.filePath(QStringLiteral("Gone.reqdeck"))));
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("v")));
    QVERIFY(manager.saveWorkspace());

    const QString secretsFile = SecretStore(manager.secretsDir()).filePath(manager.workspaceData().id);
    QVERIFY(QFile::exists(secretsFile));

    WorkspaceData changed = manager.workspaceData();
    changed.environments[0].variables.removeAt(1);
    manager.updateWorkspaceData(changed);
    QVERIFY(manager.saveWorkspace());
    QVERIFY(!QFile::exists(secretsFile));
}

void WorkspaceManagerTest::saveAsGetsOwnSecrets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString firstPath = dir.filePath(QStringLiteral("First.reqdeck"));
    const QString secretsDir = dir.filePath(QStringLiteral("secrets"));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(secretsDir);
    QVERIFY(manager.createWorkspaceAtPath(firstPath));
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("original")));
    QVERIFY(manager.saveWorkspace());
    const QString firstId = manager.workspaceData().id;

    QVERIFY(manager.saveWorkspaceAs(dir.filePath(QStringLiteral("Copy.reqdeck"))));
    const QString copyId = manager.workspaceData().id;
    QVERIFY(copyId != firstId);
    QVERIFY(!QUuid::fromString(copyId).isNull());

    const SecretStore store(secretsDir);
    QVERIFY(QFile::exists(store.filePath(firstId)));
    QVERIFY(QFile::exists(store.filePath(copyId)));

    WorkspaceData changed = manager.workspaceData();
    changed.environments[0].variables[1].value = QStringLiteral("changed");
    manager.updateWorkspaceData(changed);
    QVERIFY(manager.saveWorkspace());

    QVERIFY(manager.openWorkspace(firstPath));
    QCOMPARE(manager.workspaceData().environments.at(0).variables.at(1).value,
             QStringLiteral("original"));
}

/*!
 * \brief Creates \a dir/Foreign.reqdeck whose secrets file was encrypted on another computer.
 * \return The path of the secrets file, or an empty string on failure.
 */
QString WorkspaceManagerTest::makeForeignWorkspace(QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("Foreign.reqdeck"));
    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    if (!manager.createWorkspaceAtPath(path))
        return {};
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QString()));
    if (!manager.saveWorkspace())
        return {};
    const SecretStore other(manager.secretsDir(), SecretCipher(QByteArrayLiteral("another computer")));
    const WorkspaceData data = withSecret(manager.workspaceData(), QStringLiteral("from-elsewhere"));
    if (!other.save(data))
        return {};
    return other.filePath(data.id);
}

void WorkspaceManagerTest::migratesPlainFileOnOpen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Plain.reqdeck"));
    QString secretsFile;
    {
        TestableWorkspaceManager manager;
        manager.setSettings(makeSettings(dir, &manager));
        manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
        QVERIFY(manager.createWorkspaceAtPath(path));
        manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("x")));
        QVERIFY(manager.saveWorkspace());
        secretsFile = SecretStore(manager.secretsDir()).filePath(manager.workspaceData().id);
    }
    QFile plain(secretsFile);
    QVERIFY(plain.open(QIODevice::WriteOnly));
    plain.write("{\"schema\":1,\"environments\":{\"e1\":{\"token\":[\"plain-one\"]}}}");
    plain.close();

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(path));

    QCOMPARE(manager.workspaceData().environments.at(0).variables.at(1).value, QStringLiteral("plain-one"));
    QVERIFY(!readFile(secretsFile).contains("plain-one"));
    QVERIFY(storedSecrets(secretsFile).contains("plain-one"));
    QVERIFY(!manager.dirty());
}

void WorkspaceManagerTest::unavailableSecretsDoNotBlockSaving()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString secretsFile = makeForeignWorkspace(dir);
    QVERIFY(!secretsFile.isEmpty());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QSignalSpy notes(&manager, &WorkspaceManager::notification);
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Foreign.reqdeck"))));

    QVERIFY(manager.workspaceData().environments.at(0).variables.at(1).value.isEmpty());
    QVERIFY(!manager.secretsUnread());
    QVERIFY(std::any_of(notes.cbegin(), notes.cend(), [](const QList<QVariant> &note) {
        return note.at(1).toString().contains(QStringLiteral("another computer"));
    }));

    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("entered-here")));
    QVERIFY(manager.saveWorkspace());
    QVERIFY(storedSecrets(secretsFile).contains("entered-here"));
}

void WorkspaceManagerTest::unavailableFileStaysUntilValues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString secretsFile = makeForeignWorkspace(dir);
    QVERIFY(!secretsFile.isEmpty());
    const QByteArray foreign = readFile(secretsFile);

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Foreign.reqdeck"))));
    WorkspaceData changed = manager.workspaceData();
    changed.environments[0].variables[0].value = QStringLiteral("https://other");
    manager.updateWorkspaceData(changed);

    QVERIFY(manager.saveWorkspace());
    QCOMPARE(readFile(secretsFile), foreign);
}

void WorkspaceManagerTest::unchangedSaveKeepsFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Same.reqdeck"));
    {
        TestableWorkspaceManager manager;
        manager.setSettings(makeSettings(dir, &manager));
        manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
        QVERIFY(manager.createWorkspaceAtPath(path));
        manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("kept")));
        QVERIFY(manager.saveWorkspace());
    }
    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(path));
    const QString secretsFile = SecretStore(manager.secretsDir()).filePath(manager.workspaceData().id);
    const QByteArray workspaceBefore = readFile(path);
    const QByteArray secretsBefore = readFile(secretsFile);
    QVERIFY(!secretsBefore.isEmpty());

    QVERIFY(manager.saveWorkspace());

    QCOMPARE(readFile(path), workspaceBefore);
    QCOMPARE(readFile(secretsFile), secretsBefore);
}

/*!
 * \brief Creates \a dir/Plain.reqdeck with a secrets file of schema 1.
 * \return The path of the secrets file, or an empty string on failure.
 */
QString WorkspaceManagerTest::makePlainWorkspace(QTemporaryDir &dir)
{
    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    if (!manager.createWorkspaceAtPath(dir.filePath(QStringLiteral("Plain.reqdeck"))))
        return {};
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("x")));
    if (!manager.saveWorkspace())
        return {};
    const QString secretsFile = SecretStore(manager.secretsDir()).filePath(manager.workspaceData().id);
    QFile plain(secretsFile);
    if (!plain.open(QIODevice::WriteOnly))
        return {};
    plain.write("{\"schema\":1,\"environments\":{\"e1\":{\"token\":[\"plain-one\"]}}}");
    return secretsFile;
}

void WorkspaceManagerTest::backupBeforeFormatChange()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!makePlainWorkspace(dir).isEmpty());
    const QString path = dir.filePath(QStringLiteral("Plain.reqdeck"));
    const QByteArray workspaceBefore = readFile(path);

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QSignalSpy changed(&manager, &WorkspaceManager::previousCopyChanged);
    QVERIFY(manager.openWorkspace(path));

    QVERIFY(manager.hasPreviousCopy());
    QVERIFY(changed.count() > 0);
    QVERIFY(!manager.previousCopyDate().isEmpty());
    const std::optional<WorkspaceBackup::Copy> copy =
        WorkspaceBackup(dir.filePath(QStringLiteral("backups"))).load(manager.workspaceData().id);
    QVERIFY(copy);
    QCOMPARE(copy->workspace, workspaceBefore);
    QVERIFY(copy->secrets);
    const QByteArray sealed = QByteArray::fromBase64(
        QJsonDocument::fromJson(*copy->secrets).object().value(QLatin1StringView("data")).toString().toLatin1());
    QVERIFY(SecretCipher().unprotect(sealed).value_or(QByteArray()).contains("plain-one"));
}

void WorkspaceManagerTest::backupKeepsSecretsEncrypted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!makePlainWorkspace(dir).isEmpty());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Plain.reqdeck"))));

    const QString folder = WorkspaceBackup(dir.filePath(QStringLiteral("backups"))).folderPath(manager.workspaceData().id);
    const QStringList files = QDir(folder).entryList(QDir::Files);
    QVERIFY(!files.isEmpty());
    for (const QString &name : files)
        QVERIFY2(!readFile(QDir(folder).filePath(name)).contains("plain-one"), qPrintable(name));
}

void WorkspaceManagerTest::noBackupWithoutFormatChange()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Current.reqdeck"));
    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.createWorkspaceAtPath(path));
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("v")));
    QVERIFY(manager.saveWorkspace());
    QVERIFY(manager.openWorkspace(path));
    QVERIFY(manager.saveWorkspace());

    QVERIFY(!manager.hasPreviousCopy());
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("backups"))));
}

void WorkspaceManagerTest::restoreBringsBackPreviousCopy()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString secretsFile = makePlainWorkspace(dir);
    QVERIFY(!secretsFile.isEmpty());
    const QString path = dir.filePath(QStringLiteral("Plain.reqdeck"));
    const QByteArray workspaceBefore = readFile(path);

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(path));
    WorkspaceData changed = manager.workspaceData();
    changed.environments[0].variables[0].value = QStringLiteral("https://changed");
    changed.environments[0].variables[1].value = QStringLiteral("changed-secret");
    manager.updateWorkspaceData(changed);
    QVERIFY(manager.saveWorkspace());
    QVERIFY(readFile(path) != workspaceBefore);

    QVERIFY(manager.restorePreviousCopy());

    QCOMPARE(readFile(path), workspaceBefore);
    QCOMPARE(manager.workspaceData().environments.at(0).variables.at(0).value, QStringLiteral("https://h"));
    QCOMPARE(manager.workspaceData().environments.at(0).variables.at(1).value, QStringLiteral("plain-one"));
    QVERIFY(storedSecrets(secretsFile).contains("plain-one"));
    QVERIFY(!manager.dirty());
    QVERIFY(manager.hasPreviousCopy());
}

void WorkspaceManagerTest::restoreWithoutCopyFails()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("None.reqdeck"));
    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.createWorkspaceAtPath(path));
    const QByteArray before = readFile(path);
    QSignalSpy errors(&manager, &WorkspaceManager::workspaceError);

    QVERIFY(!manager.restorePreviousCopy());

    QCOMPARE(errors.count(), 1);
    QCOMPARE(readFile(path), before);
}

void WorkspaceManagerTest::inlineSecretSurvivesUnchangedSave()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Inline.reqdeck"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"schema\": 1, \"id\": \"3f2b8c1e-7a44-4a8e-9a55-0c1d2e3f4a5b\", \"environments\": [{\"id\": "
               "\"e1\", \"name\": \"E\", \"variables\": [{\"key\": \"k\", \"value\": \"inline\", \"secret\": true}]}]}");
    file.close();
    {
        TestableWorkspaceManager manager;
        manager.setSettings(makeSettings(dir, &manager));
        manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
        QVERIFY(manager.openWorkspace(path));
        QVERIFY(manager.saveWorkspace());
    }

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(path));
    QCOMPARE(manager.workspaceData().environments.at(0).variables.at(0).value, QStringLiteral("inline"));
}

void WorkspaceManagerTest::migrationCopyKeepsEveryValue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString secretsFile = makePlainWorkspace(dir);
    QVERIFY(!secretsFile.isEmpty());
    QFile plain(secretsFile);
    QVERIFY(plain.open(QIODevice::WriteOnly));
    plain.write("{\"schema\":1,\"environments\":{\"e1\":{\"token\":[\"plain-one\"]},"
                "\"gone\":{\"old\":[\"orphan-value\"]}}}");
    plain.close();

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Plain.reqdeck"))));

    const std::optional<WorkspaceBackup::Copy> copy =
        WorkspaceBackup(dir.filePath(QStringLiteral("backups"))).load(manager.workspaceData().id);
    QVERIFY(copy && copy->secrets);
    QVERIFY(!copy->secrets->contains("orphan-value"));
    const QByteArray sealed = QByteArray::fromBase64(
        QJsonDocument::fromJson(*copy->secrets).object().value(QLatin1StringView("data")).toString().toLatin1());
    const QByteArray values = SecretCipher().unprotect(sealed).value_or(QByteArray());
    QVERIFY2(values.contains("orphan-value"), values.constData());
    QVERIFY(values.contains("plain-one"));
}

void WorkspaceManagerTest::unavailableFileCopiedBeforeReplaced()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString secretsFile = makeForeignWorkspace(dir);
    QVERIFY(!secretsFile.isEmpty());
    const QByteArray foreign = readFile(secretsFile);

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Foreign.reqdeck"))));
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("entered-here")));
    QVERIFY(manager.saveWorkspace());

    const std::optional<WorkspaceBackup::Copy> copy =
        WorkspaceBackup(dir.filePath(QStringLiteral("backups"))).load(manager.workspaceData().id);
    QVERIFY(copy && copy->secrets);
    QCOMPARE(*copy->secrets, foreign);
    QVERIFY(manager.hasPreviousCopy());
}

void WorkspaceManagerTest::backupRejectsInvalidId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const WorkspaceBackup backup(dir.filePath(QStringLiteral("backups")));
    QString error;

    QVERIFY(!backup.save(QStringLiteral("../outside"), {QByteArrayLiteral("{}"), std::nullopt, {}}, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!backup.exists(QStringLiteral("../outside")));
    QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("outside"))));
}

QString WorkspaceManagerTest::makeLockedWorkspace(QTemporaryDir &dir, QByteArray &secrets)
{
    const QString path = dir.filePath(QStringLiteral("Locked.reqdeck"));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    if (!manager.createWorkspaceAtPath(path))
        return {};
    WorkspaceData data = withSecret(manager.workspaceData(), QStringLiteral("stored-one"));
    data.environments[0].variables.append(
        {QStringLiteral("second"), QStringLiteral("stored-two"), true, true});
    manager.updateWorkspaceData(data);
    if (!manager.saveWorkspace())
        return {};

    const QString secretsFile = SecretStore(manager.secretsDir()).filePath(data.id);
    secrets = readFile(secretsFile);
    // A directory at the file's path cannot be opened, like a locked file.
    if (!QFile::remove(secretsFile) || !QDir().mkpath(secretsFile))
        return {};
    return secretsFile;
}

void WorkspaceManagerTest::openSamplesOpensSeededWorkspace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Samples.reqdeck"));
    WorkspaceData data;
    data.id = newItemId();
    data.displayName = QStringLiteral("Samples");
    QVERIFY(WorkspaceSerializer::save(path, data));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    manager.setSamplesPath(path);

    QVERIFY(manager.openSamples());

    QCOMPARE(manager.activeWorkspaceName(), QStringLiteral("Samples"));
    QVERIFY(!manager.dirty());
}

void WorkspaceManagerTest::openSamplesMissingFileReportsError()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSamplesPath(dir.filePath(QStringLiteral("missing.reqdeck")));
    QSignalSpy errorSpy(&manager, &WorkspaceManager::workspaceError);

    QVERIFY(!manager.openSamples());

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!manager.hasActiveWorkspace());
}

void WorkspaceManagerTest::unreadableSecretsBlockSave()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray secrets;
    const QString secretsFile = makeLockedWorkspace(dir, secrets);
    QVERIFY(!secretsFile.isEmpty());
    const QString path = dir.filePath(QStringLiteral("Locked.reqdeck"));
    const QByteArray before = readFile(path);

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(path));

    WorkspaceData changed = manager.workspaceData();
    changed.settings.insert(QStringLiteral("changed"), true);
    manager.updateWorkspaceData(changed);

    QSignalSpy errorSpy(&manager, &WorkspaceManager::workspaceError);
    QVERIFY(!manager.saveWorkspace());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(readFile(path), before);
    QVERIFY(QFileInfo(secretsFile).isDir());
    QVERIFY(manager.dirty());
}

void WorkspaceManagerTest::unreadableSecretsAreRecoveredOnSave()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray secrets;
    const QString secretsFile = makeLockedWorkspace(dir, secrets);
    QVERIFY(!secretsFile.isEmpty());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Locked.reqdeck"))));
    QVERIFY(manager.workspaceData().environments.at(0).variables.at(1).value.isEmpty());

    QVERIFY(QDir().rmdir(secretsFile));
    QFile file(secretsFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(secrets);
    file.close();

    QVERIFY(manager.saveWorkspace());
    QCOMPARE(manager.workspaceData().environments.at(0).variables.at(1).value,
             QStringLiteral("stored-one"));
    QVERIFY(storedSecrets(secretsFile).contains("stored-one"));
    QVERIFY(storedSecrets(secretsFile).contains("stored-two"));
}

void WorkspaceManagerTest::retrySecretsKeepsStateWhileLocked()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray secrets;
    QVERIFY(!makeLockedWorkspace(dir, secrets).isEmpty());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Locked.reqdeck"))));
    QVERIFY(manager.secretsUnread());

    QVERIFY(!manager.retrySecrets());

    QVERIFY(manager.secretsUnread());
}

void WorkspaceManagerTest::retrySecretsFillsValuesOnceReadable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray secrets;
    const QString secretsFile = makeLockedWorkspace(dir, secrets);
    QVERIFY(!secretsFile.isEmpty());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Locked.reqdeck"))));
    QSignalSpy unread(&manager, &WorkspaceManager::secretsUnreadChanged);

    QVERIFY(QDir().rmdir(secretsFile));
    QFile file(secretsFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(secrets);
    file.close();

    QVERIFY(manager.retrySecrets());

    QVERIFY(!manager.secretsUnread());
    QCOMPARE(unread.count(), 1);
    QCOMPARE(manager.workspaceData().environments.at(0).variables.at(1).value,
             QStringLiteral("stored-one"));
    QVERIFY(!manager.dirty());
}

void WorkspaceManagerTest::recoveryKeepsReenteredValue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QByteArray secrets;
    const QString secretsFile = makeLockedWorkspace(dir, secrets);
    QVERIFY(!secretsFile.isEmpty());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.openWorkspace(dir.filePath(QStringLiteral("Locked.reqdeck"))));

    WorkspaceData changed = manager.workspaceData();
    changed.environments[0].variables[1].value = QStringLiteral("re-entered");
    manager.updateWorkspaceData(changed);

    QVERIFY(QDir().rmdir(secretsFile));
    QFile file(secretsFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(secrets);
    file.close();

    QVERIFY(manager.saveWorkspace());
    QVERIFY(storedSecrets(secretsFile).contains("re-entered"));
    QVERIFY(!storedSecrets(secretsFile).contains("stored-one"));
    QVERIFY(storedSecrets(secretsFile).contains("stored-two"));
}

void WorkspaceManagerTest::setAsideFailureBlocksSave()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Stuck.reqdeck"));
    const QString secretsDir = dir.filePath(QStringLiteral("secrets"));

    QString secretsFile;
    {
        TestableWorkspaceManager manager;
        manager.setSettings(makeSettings(dir, &manager));
        manager.setSecretsDir(secretsDir);
        QVERIFY(manager.createWorkspaceAtPath(path));
        manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("v")));
        QVERIFY(manager.saveWorkspace());
        secretsFile = SecretStore(secretsDir).filePath(manager.workspaceData().id);
    }

    QFile secrets(secretsFile);
    QVERIFY(secrets.open(QIODevice::WriteOnly));
    secrets.write("{oops");
    secrets.close();

    // Non-empty directories block both the old copy and the temporary name.
    for (const QString &suffix : {QStringLiteral(".corrupt"), QStringLiteral(".corrupt.new")}) {
        QVERIFY(QDir().mkpath(secretsFile + suffix + QStringLiteral("/keep")));
    }

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(secretsDir);
    QVERIFY(manager.openWorkspace(path));
    QVERIFY(!manager.saveWorkspace());
    QCOMPARE(readFile(secretsFile), QByteArray("{oops"));
}

void WorkspaceManagerTest::workspaceWriteFailureRestoresSecrets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("ReadOnly.reqdeck"));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.createWorkspaceAtPath(path));
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("keep-me")));
    QVERIFY(manager.saveWorkspace());

    const QString secretsFile =
        SecretStore(manager.secretsDir()).filePath(manager.workspaceData().id);
    const QByteArray secrets = readFile(secretsFile);

    WorkspaceData changed = manager.workspaceData();
    changed.environments[0].variables.removeAt(1);
    manager.updateWorkspaceData(changed);

    const QFile::Permissions original = QFile::permissions(path);
    QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::ReadUser));
    const bool saved = manager.saveWorkspace();
    QFile::setPermissions(path, original);

    QVERIFY(!saved);
    QCOMPARE(readFile(secretsFile), secrets);
    QVERIFY(manager.dirty());
}

void WorkspaceManagerTest::failedSaveAsLeavesNoSecrets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString secretsDir = dir.filePath(QStringLiteral("secrets"));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(secretsDir);
    QVERIFY(manager.createWorkspaceAtPath(dir.filePath(QStringLiteral("First.reqdeck"))));
    manager.updateWorkspaceData(withSecret(manager.workspaceData(), QStringLiteral("v")));
    QVERIFY(manager.saveWorkspace());

    const QString blocked = dir.filePath(QStringLiteral("blocked"));
    QFile blocker(blocked);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    QVERIFY(!manager.saveWorkspaceAs(blocked + QStringLiteral("/Copy.reqdeck")));
    QCOMPARE(QDir(secretsDir).entryList(QDir::Files).size(), 1);
}

void WorkspaceManagerTest::updateKeepsWorkspaceId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.createWorkspaceAtPath(dir.filePath(QStringLiteral("Keep.reqdeck"))));
    const QString id = manager.workspaceData().id;

    WorkspaceData changed = manager.workspaceData();
    changed.id = newItemId();
    changed.settings.insert(QStringLiteral("changed"), true);
    manager.updateWorkspaceData(changed);
    QCOMPARE(manager.workspaceData().id, id);
    QVERIFY(manager.dirty());
    QVERIFY(manager.saveWorkspace());

    changed = manager.workspaceData();
    changed.id.clear();
    manager.updateWorkspaceData(changed);
    QCOMPARE(manager.workspaceData().id, id);
    QVERIFY(!manager.dirty());
}

void WorkspaceManagerTest::saveKeepsWorkspaceId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("Same.reqdeck"));

    TestableWorkspaceManager manager;
    manager.setSettings(makeSettings(dir, &manager));
    manager.setSecretsDir(dir.filePath(QStringLiteral("secrets")));
    QVERIFY(manager.createWorkspaceAtPath(path));
    const QString id = manager.workspaceData().id;

    QVERIFY(manager.saveWorkspace());
    QVERIFY(manager.saveWorkspaceAs(path));
    QCOMPARE(manager.workspaceData().id, id);
}

QTEST_GUILESS_MAIN(WorkspaceManagerTest)

#include "tst_workspacemanager.moc"
