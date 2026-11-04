// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "framework/apppaths.h"
#include "productinfo.h"

/*!
 * \internal
 * \brief Writes \a content to \a path, replacing any existing file.
 */
static bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
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

    /*! Verifies that secret values are stored below the data directory. */
    void secretsDirIsUnderDataDir();

    /*! Verifies that the request history is stored below the data directory. */
    void historyFileIsUnderDataDir();

    /*! Verifies where the seeded samples workspace lives. */
    void samplesPathIsUnderDataDir();

    /*! Verifies that an existing target is never replaced. */
    void seedFileCopiesOnce();

    /*! Verifies that a seed from a read-only source can be edited. */
    void seededFileIsWritable();

    /*! Verifies the failure for a missing source. */
    void seedFileReportsMissingSource();

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

void AppPathsTest::secretsDirIsUnderDataDir()
{
    const AppPaths &paths = AppPaths::instance();
    QCOMPARE(paths.secretsDir(), paths.dataDir() + QStringLiteral("/secrets"));
}

void AppPathsTest::historyFileIsUnderDataDir()
{
    const AppPaths &paths = AppPaths::instance();
    QCOMPARE(paths.historyFilePath(), paths.dataDir() + QStringLiteral("/history.json"));
}

void AppPathsTest::samplesPathIsUnderDataDir()
{
    const AppPaths &paths = AppPaths::instance();
    QCOMPARE(paths.samplesWorkspacePath(),
             paths.dataDir() + QStringLiteral("/samples/Samples.reqdeck"));
}

void AppPathsTest::seedFileCopiesOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString first = dir.filePath(QStringLiteral("first.txt"));
    const QString second = dir.filePath(QStringLiteral("second.txt"));
    const QString target = dir.filePath(QStringLiteral("nested/target.txt"));
    QVERIFY(writeFile(first, "first"));
    QVERIFY(writeFile(second, "second"));

    QString error;
    QVERIFY2(AppPaths::seedFile(first, target, &error), qPrintable(error));
    QCOMPARE(readFile(target), QByteArray("first"));

    QVERIFY(AppPaths::seedFile(second, target));
    QCOMPARE(readFile(target), QByteArray("first"));
}

void AppPathsTest::seededFileIsWritable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("source.txt"));
    const QString target = dir.filePath(QStringLiteral("target.txt"));
    QVERIFY(writeFile(source, "seed"));

    const QFile::Permissions original = QFile::permissions(source);
    QVERIFY(QFile::setPermissions(source, QFile::ReadOwner | QFile::ReadUser));

    const bool seeded = AppPaths::seedFile(source, target);
    QFile::setPermissions(source, original);
    QVERIFY(seeded);

    QFile file(target);
    QVERIFY(file.open(QIODevice::WriteOnly));
}

void AppPathsTest::seedFileReportsMissingSource()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    QVERIFY(!AppPaths::seedFile(dir.filePath(QStringLiteral("missing")),
                                dir.filePath(QStringLiteral("target")), &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(AppPathsTest)

#include "tst_apppaths.moc"
