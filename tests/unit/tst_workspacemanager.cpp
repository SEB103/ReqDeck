// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>
#include <QtTest>

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

private:
    /*! Builds a settings store inside \a dir for one test. */
    static QSettings *makeSettings(QTemporaryDir &dir, QObject *parent);
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

QTEST_GUILESS_MAIN(WorkspaceManagerTest)

#include "tst_workspacemanager.moc"
