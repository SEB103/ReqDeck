// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtTest>

#include "framework/apppaths.h"
#include "productinfo.h"

/*!
 * \internal
 * \brief Verifies the installed-mode path policy of AppPaths.
 *
 * The test executable has no portable.ini marker next to it, so AppPaths
 * resolves installed mode. QStandardPaths test mode keeps the resolved per-user
 * locations away from the real user profile.
 */
class AppPathsTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Sets the product identity and enables QStandardPaths test mode. */
    void initTestCase();

    /*! Verifies that installed mode is selected without a portable marker. */
    void installedModeWithoutPortableMarker();

    /*! Verifies that every directory is absolute and normalized. */
    void directoriesAreAbsoluteAndClean();

    /*! Verifies the installed-mode base locations. */
    void installedModeUsesStandardLocations();

    /*! Verifies that workspaces default to Documents/<product identifier>. */
    void workspacesDefaultToDocumentsFolder();

    /*! Verifies that a second initialize() call does not change the result. */
    void initializeIsIdempotent();
};

void AppPathsTest::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral(PRODUCT_IDENTIFIER));
    QCoreApplication::setApplicationName(QStringLiteral(PRODUCT_IDENTIFIER));
    QStandardPaths::setTestModeEnabled(true);
    AppPaths::instance().initialize();
}

void AppPathsTest::installedModeWithoutPortableMarker()
{
    QVERIFY(!QFileInfo::exists(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("portable.ini"))));
    QVERIFY(!AppPaths::instance().isPortable());
    QCOMPARE(AppPaths::instance().seedDir(),
             QDir::cleanPath(QCoreApplication::applicationDirPath()));
}

void AppPathsTest::directoriesAreAbsoluteAndClean()
{
    const AppPaths &paths = AppPaths::instance();
    const QStringList dirs {paths.seedDir(), paths.configDir(), paths.dataDir(),
                            paths.localDataDir(), paths.logDir(),
                            paths.defaultWorkspacesDir()};
    for (const QString &dir : dirs) {
        QVERIFY2(!dir.isEmpty(), "directory must not be empty");
        QVERIFY2(QDir::isAbsolutePath(dir), qPrintable(dir));
        QCOMPARE(dir, QDir::cleanPath(dir));
    }
}

void AppPathsTest::installedModeUsesStandardLocations()
{
    const AppPaths &paths = AppPaths::instance();
    QCOMPARE(paths.configDir(),
             QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)));
    QCOMPARE(paths.dataDir(),
             QDir::cleanPath(
                 QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)));
    QCOMPARE(paths.localDataDir(), paths.dataDir());
    QCOMPARE(paths.logDir(), QDir(paths.localDataDir()).filePath(QStringLiteral("log")));
}

void AppPathsTest::workspacesDefaultToDocumentsFolder()
{
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QCOMPARE(AppPaths::instance().defaultWorkspacesDir(),
             QDir::cleanPath(QDir(documents).filePath(QStringLiteral(PRODUCT_IDENTIFIER))));
}

void AppPathsTest::initializeIsIdempotent()
{
    const QString before = AppPaths::instance().configDir();
    AppPaths::instance().initialize();
    QCOMPARE(AppPaths::instance().configDir(), before);
}

QTEST_GUILESS_MAIN(AppPathsTest)

#include "tst_apppaths.moc"
