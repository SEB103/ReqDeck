// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "productinfo.h"
#include "updatecontroller.h"

/*!
 * \internal
 * \brief Unit tests for UpdateController: version comparison, preference storage
 *        and the installed/portable update actions.
 *
 * The network path is not exercised here (it requires a reachable release
 * server); the "update available" state is produced offline through
 * applyReleasePayload(). The update actions run against a fake launch
 * environment, so no Maintenance Tool process and no browser is ever started.
 */
class TestUpdateController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    /*! Verifies the semantic version ordering used to decide UpToDate vs UpdateAvailable. */
    void compareVersions_data();
    void compareVersions();

    /*! Verifies that the automatic-check preference round-trips through QSettings. */
    void checkAutomaticallyPersists();

    /*! Verifies that the update-check gate follows the product configuration. */
    void featureEnabledByProductConfiguration();

    /*! Verifies that a check with the feature disabled reports NotConfigured offline. */
    void disabledCheckReportsNotConfigured();

    /*! Verifies the pure action choice for every installation mode. */
    void resolveUpdateAction_data();
    void resolveUpdateAction();

    /*! Verifies that an installed copy starts the Maintenance Tool and requests quitting. */
    void installedWithToolLaunchesUpdaterAndRequestsQuit();

    /*! Verifies that a failed process start keeps the application running with an error. */
    void installLaunchFailureKeepsApplicationRunning();

    /*! Verifies that a portable copy never starts the tool and opens the release page. */
    void portableOpensDownloadPage();

    /*! Verifies the development fallback: installed mode without a Maintenance Tool. */
    void installedWithoutToolFallsBackToDownloadPage();

    /*! Verifies that only HTTPS release URLs are handed to the system. */
    void openDownloadPageRejectsNonHttpsUrl();

    /*! Verifies that no action is offered while no update is available. */
    void noActionWithoutAvailableUpdate();

private:
    /*! Feeds a "latest release" payload announcing \a tag at \a htmlUrl. */
    static void announceRelease(UpdateController &controller, const QString &tag,
                                const QString &htmlUrl);

    /*! Creates an empty fake Maintenance Tool file under \a dir and returns its path. */
    static QString createFakeTool(const QTemporaryDir &dir);
};

void TestUpdateController::initTestCase()
{
    // Give the controller a concrete running version to compare against.
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
}

void TestUpdateController::announceRelease(UpdateController &controller, const QString &tag,
                                           const QString &htmlUrl)
{
    const QByteArray payload = QStringLiteral(R"({"tag_name":"%1","html_url":"%2"})")
                                   .arg(tag, htmlUrl)
                                   .toUtf8();
    controller.applyReleasePayload(payload);
}

QString TestUpdateController::createFakeTool(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(QStringLiteral("FakeMaintenanceTool.exe"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();
    file.write("stub");
    file.close();
    return QDir::cleanPath(path);
}

void TestUpdateController::compareVersions_data()
{
    QTest::addColumn<QString>("lhs");
    QTest::addColumn<QString>("rhs");
    QTest::addColumn<int>("sign");

    QTest::newRow("equal") << "1.0.0" << "1.0.0" << 0;
    QTest::newRow("patch-greater") << "1.0.1" << "1.0.0" << 1;
    QTest::newRow("patch-less") << "1.0.0" << "1.0.1" << -1;
    QTest::newRow("minor-greater") << "1.2.0" << "1.1.9" << 1;
    QTest::newRow("major-greater") << "2.0.0" << "1.9.9" << 1;
    QTest::newRow("leading-v") << "v1.2.0" << "1.2.0" << 0;
    QTest::newRow("missing-segment-equal") << "1.0" << "1.0.0" << 0;
    QTest::newRow("missing-segment-less") << "1.0" << "1.0.1" << -1;
    QTest::newRow("prerelease-suffix") << "1.2.0-rc1" << "1.2.0" << 0;
}

void TestUpdateController::compareVersions()
{
    QFETCH(QString, lhs);
    QFETCH(QString, rhs);
    QFETCH(int, sign);

    const int result = UpdateController::compareVersions(lhs, rhs);
    if (sign == 0)
        QCOMPARE(result, 0);
    else if (sign < 0)
        QVERIFY(result < 0);
    else
        QVERIFY(result > 0);
}

void TestUpdateController::checkAutomaticallyPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString iniPath = dir.filePath(QStringLiteral("settings.ini"));

    {
        QSettings settings(iniPath, QSettings::IniFormat);
        UpdateController controller(&settings);
        QCOMPARE(controller.checkAutomatically(), false);

        QSignalSpy spy(&controller, &UpdateController::checkAutomaticallyChanged);
        controller.setCheckAutomatically(true);
        QCOMPARE(controller.checkAutomatically(), true);
        QCOMPARE(spy.count(), 1);

        // Setting the same value again must not emit a redundant change.
        controller.setCheckAutomatically(true);
        QCOMPARE(spy.count(), 1);
    }

    // A fresh controller reads the persisted preference back.
    QSettings reopened(iniPath, QSettings::IniFormat);
    UpdateController restored(&reopened);
    QCOMPARE(restored.checkAutomatically(), true);
}

void TestUpdateController::featureEnabledByProductConfiguration()
{
    // packaging/product.json update.enabled flows through the generated
    // productinfo.h into the controller.
    UpdateController controller(nullptr);
    QCOMPARE(controller.featureEnabled(), PRODUCT_UPDATE_ENABLED);
    QCOMPARE(controller.status(), UpdateController::Status::Idle);
}

void TestUpdateController::disabledCheckReportsNotConfigured()
{
    UpdateController controller(nullptr);
    if (controller.featureEnabled())
        QSKIP("Update checking is enabled in packaging/product.json; a check would need the network.");

    controller.checkNow();
    QCOMPARE(controller.status(), UpdateController::Status::NotConfigured);
    QVERIFY(!controller.isChecking());
    QVERIFY(!controller.isUpdateAvailable());
}

void TestUpdateController::resolveUpdateAction_data()
{
    QTest::addColumn<bool>("portable");
    QTest::addColumn<bool>("toolExists");
    QTest::addColumn<UpdateController::UpdateAction>("expected");

    QTest::newRow("installed-with-tool")
        << false << true << UpdateController::UpdateAction::InstallUpdate;
    QTest::newRow("installed-without-tool")
        << false << false << UpdateController::UpdateAction::OpenDownloadPage;
    QTest::newRow("portable-with-tool")
        << true << true << UpdateController::UpdateAction::OpenDownloadPage;
    QTest::newRow("portable-without-tool")
        << true << false << UpdateController::UpdateAction::OpenDownloadPage;
}

void TestUpdateController::resolveUpdateAction()
{
    QFETCH(bool, portable);
    QFETCH(bool, toolExists);
    QFETCH(UpdateController::UpdateAction, expected);

    QCOMPARE(UpdateController::resolveUpdateAction(portable, toolExists), expected);
}

void TestUpdateController::installedWithToolLaunchesUpdaterAndRequestsQuit()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString tool = createFakeTool(dir);
    QVERIFY(!tool.isEmpty());

    QString launchedProgram;
    QStringList launchedArguments;
    QString launchedWorkingDir;
    int urlOpens = 0;

    UpdateController controller(nullptr);
    UpdateController::LaunchEnvironment env;
    env.portable = false;
    env.maintenanceToolPath = tool;
    env.startProcess = [&](const QString &program, const QStringList &arguments,
                           const QString &workingDir) {
        launchedProgram = program;
        launchedArguments = arguments;
        launchedWorkingDir = workingDir;
        return true;
    };
    env.openUrl = [&](const QUrl &) { ++urlOpens; return true; };
    controller.setLaunchEnvironment(env);

    QSignalSpy quitSpy(&controller, &UpdateController::quitRequested);
    announceRelease(controller, QStringLiteral("v1.1.0"),
                    QStringLiteral("https://github.com/SEB103/ReqDeck/releases/tag/v1.1.0"));

    QCOMPARE(controller.status(), UpdateController::Status::UpdateAvailable);
    QCOMPARE(controller.updateAction(), UpdateController::UpdateAction::InstallUpdate);
    QVERIFY(controller.canInstallUpdate());
    QCOMPARE(controller.maintenanceToolPath(), tool);

    QVERIFY(controller.installUpdate());
    QCOMPARE(launchedProgram, tool);
    QCOMPARE(launchedArguments, QStringList {QStringLiteral("--start-updater")});
    QCOMPARE(QDir::cleanPath(launchedWorkingDir), QDir::cleanPath(dir.path()));
    QCOMPARE(quitSpy.count(), 1);
    QVERIFY(controller.actionError().isEmpty());
    QCOMPARE(urlOpens, 0);
}

void TestUpdateController::installLaunchFailureKeepsApplicationRunning()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString tool = createFakeTool(dir);
    QVERIFY(!tool.isEmpty());

    UpdateController controller(nullptr);
    UpdateController::LaunchEnvironment env;
    env.portable = false;
    env.maintenanceToolPath = tool;
    env.startProcess = [](const QString &, const QStringList &, const QString &) { return false; };
    controller.setLaunchEnvironment(env);

    QSignalSpy quitSpy(&controller, &UpdateController::quitRequested);
    QSignalSpy errorSpy(&controller, &UpdateController::actionErrorChanged);
    announceRelease(controller, QStringLiteral("v1.1.0"),
                    QStringLiteral("https://github.com/SEB103/ReqDeck/releases/tag/v1.1.0"));

    QVERIFY(controller.canInstallUpdate());
    QVERIFY(!controller.installUpdate());
    QCOMPARE(quitSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!controller.actionError().isEmpty());

    // The update stays available so the user can retry or fall back manually.
    QCOMPARE(controller.status(), UpdateController::Status::UpdateAvailable);
}

void TestUpdateController::portableOpensDownloadPage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString tool = createFakeTool(dir);
    QVERIFY(!tool.isEmpty());

    int processStarts = 0;
    QUrl openedUrl;

    UpdateController controller(nullptr);
    UpdateController::LaunchEnvironment env;
    env.portable = true;
    env.maintenanceToolPath = tool; // present, but irrelevant for a portable copy
    env.startProcess = [&](const QString &, const QStringList &, const QString &) {
        ++processStarts;
        return true;
    };
    env.openUrl = [&](const QUrl &url) { openedUrl = url; return true; };
    controller.setLaunchEnvironment(env);

    QSignalSpy quitSpy(&controller, &UpdateController::quitRequested);
    const QString page = QStringLiteral("https://github.com/SEB103/ReqDeck/releases/tag/v1.1.0");
    announceRelease(controller, QStringLiteral("v1.1.0"), page);

    QCOMPARE(controller.updateAction(), UpdateController::UpdateAction::OpenDownloadPage);
    QVERIFY(!controller.canInstallUpdate());

    QVERIFY(!controller.installUpdate());
    QCOMPARE(processStarts, 0);
    QCOMPARE(quitSpy.count(), 0);
    QVERIFY(!controller.actionError().isEmpty());

    QVERIFY(controller.openDownloadPage());
    QCOMPARE(openedUrl, QUrl(page));
    QVERIFY(controller.actionError().isEmpty());
}

void TestUpdateController::installedWithoutToolFallsBackToDownloadPage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    int processStarts = 0;
    QUrl openedUrl;

    UpdateController controller(nullptr);
    UpdateController::LaunchEnvironment env;
    env.portable = false;
    env.maintenanceToolPath = dir.filePath(QStringLiteral("MissingMaintenanceTool.exe"));
    env.startProcess = [&](const QString &, const QStringList &, const QString &) {
        ++processStarts;
        return true;
    };
    env.openUrl = [&](const QUrl &url) { openedUrl = url; return true; };
    controller.setLaunchEnvironment(env);

    const QString page = QStringLiteral("https://github.com/SEB103/ReqDeck/releases/tag/v1.1.0");
    announceRelease(controller, QStringLiteral("v1.1.0"), page);

    QCOMPARE(controller.updateAction(), UpdateController::UpdateAction::OpenDownloadPage);
    QVERIFY(!controller.canInstallUpdate());
    QVERIFY(!controller.installUpdate());
    QCOMPARE(processStarts, 0);

    QVERIFY(controller.openDownloadPage());
    QCOMPARE(openedUrl, QUrl(page));
}

void TestUpdateController::openDownloadPageRejectsNonHttpsUrl()
{
    int urlOpens = 0;

    UpdateController controller(nullptr);
    UpdateController::LaunchEnvironment env;
    env.portable = true;
    env.openUrl = [&](const QUrl &) { ++urlOpens; return true; };
    controller.setLaunchEnvironment(env);

    announceRelease(controller, QStringLiteral("v1.1.0"),
                    QStringLiteral("http://example.invalid/releases/tag/v1.1.0"));
    QCOMPARE(controller.status(), UpdateController::Status::UpdateAvailable);

    QVERIFY(!controller.openDownloadPage());
    QCOMPARE(urlOpens, 0);
    QVERIFY(!controller.actionError().isEmpty());
}

void TestUpdateController::noActionWithoutAvailableUpdate()
{
    int processStarts = 0;
    int urlOpens = 0;

    UpdateController controller(nullptr);
    UpdateController::LaunchEnvironment env;
    env.portable = false;
    env.startProcess = [&](const QString &, const QStringList &, const QString &) {
        ++processStarts;
        return true;
    };
    env.openUrl = [&](const QUrl &) { ++urlOpens; return true; };
    controller.setLaunchEnvironment(env);

    // Same version as the running one: nothing to do.
    announceRelease(controller, QStringLiteral("v1.0.0"),
                    QStringLiteral("https://github.com/SEB103/ReqDeck/releases/tag/v1.0.0"));
    QCOMPARE(controller.status(), UpdateController::Status::UpToDate);
    QCOMPARE(controller.updateAction(), UpdateController::UpdateAction::None);
    QVERIFY(!controller.canInstallUpdate());

    QVERIFY(!controller.installUpdate());
    QVERIFY(!controller.openDownloadPage());
    QCOMPARE(processStarts, 0);
    QCOMPARE(urlOpens, 0);
}

QTEST_GUILESS_MAIN(TestUpdateController)

#include "tst_updatecontroller.moc"
