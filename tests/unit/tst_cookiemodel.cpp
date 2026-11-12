// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "core/http/cookiestore.h"
#include "qmlapi/cookiemodel.h"
#include "qmlapi/workspacemanager.h"

/*! Verifies the QML model of the cookies of a workspace. */
class CookieModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Creates a workspace with a fresh manager and model. */
    void init();

    /*! Drops the model, the manager, and the workspace. */
    void cleanup();

    /*! Verifies the rows and roles, and that a response's cookie shows up. */
    void showsStoredCookies();

    /*! Verifies editing a value, adding a cookie that is sent, and removing. */
    void editsCookies();

    /*! Verifies that a bad name or a missing domain is refused. */
    void refusesBadCookies();

    /*! Verifies that enabled follows the workspace setting. */
    void enabledFollowsTheWorkspace();

private:
    /*! Directory of the workspace and its secrets. */
    std::unique_ptr<QTemporaryDir> m_dir;

    /*! Real workspace manager. */
    std::unique_ptr<WorkspaceManager> m_manager;

    /*! Model under test. */
    std::unique_ptr<CookieModel> m_model;

    /*! Consistency checks on the model under test. */
    std::unique_ptr<QAbstractItemModelTester> m_tester;
};

void CookieModelTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_manager = std::make_unique<WorkspaceManager>();
    m_manager->setSecretsDir(m_dir->filePath(QStringLiteral("secrets")));
    QVERIFY(m_manager->createWorkspace(QStringLiteral("Cookies"), m_dir->path()));
    m_model = std::make_unique<CookieModel>(m_manager.get());
    m_tester = std::make_unique<QAbstractItemModelTester>(
        m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
}

void CookieModelTest::cleanup()
{
    m_tester.reset();
    m_model.reset();
    m_manager.reset();
    m_dir.reset();
}

void CookieModelTest::showsStoredCookies()
{
    QSignalSpy count(m_model.get(), &CookieModel::countChanged);
    m_manager->cookieStore()->storeFromResponse({"sid=abc; Path=/; Secure; HttpOnly", "lang=en; Max-Age=3600"},
                                                QUrl(QStringLiteral("https://shop.test/")));

    QCOMPARE(m_model->rowCount(), 2);
    QCOMPARE(m_model->property("count").toInt(), 2);
    QVERIFY(count.count() > 0);
    const QModelIndex lang = m_model->index(0);
    QCOMPARE(lang.data(CookieModel::NameRole).toString(), QStringLiteral("lang"));
    QCOMPARE(lang.data(CookieModel::DomainRole).toString(), QStringLiteral("shop.test"));
    QVERIFY(!lang.data(CookieModel::ExpiresRole).toString().isEmpty());
    const QModelIndex sid = m_model->index(1);
    QCOMPARE(sid.data(CookieModel::ValueRole).toString(), QStringLiteral("abc"));
    QVERIFY(sid.data(CookieModel::ExpiresRole).toString().isEmpty());
    QVERIFY(sid.data(CookieModel::SecureRole).toBool());
    QVERIFY(sid.data(CookieModel::HttpOnlyRole).toBool());
}

void CookieModelTest::editsCookies()
{
    m_manager->cookieStore()->storeFromResponse({"sid=abc; Path=/"}, QUrl(QStringLiteral("https://shop.test/")));

    QVERIFY(m_model->setValue(0, QStringLiteral("changed")));
    QCOMPARE(m_manager->cookieStore()->cookieHeader(QUrl(QStringLiteral("https://shop.test/x"))), QByteArray("sid=changed"));
    QVERIFY(!m_model->setValue(5, QStringLiteral("x")));

    QVERIFY(m_model->add(QStringLiteral("api.test"), QStringLiteral("token"), QStringLiteral("t1"), QString()));
    QCOMPARE(m_model->rowCount(), 2);
    QCOMPARE(m_manager->cookieStore()->cookieHeader(QUrl(QStringLiteral("https://api.test/"))), QByteArray("token=t1"));

    QVERIFY(m_model->remove(0));
    QCOMPARE(m_model->rowCount(), 1);
    QVERIFY(!m_model->remove(3));
    m_model->removeAll();
    QCOMPARE(m_model->rowCount(), 0);
}

void CookieModelTest::refusesBadCookies()
{
    QVERIFY(!m_model->add(QString(), QStringLiteral("a"), QStringLiteral("1"), QString()));
    QVERIFY(!m_model->add(QStringLiteral("a.test"), QString(), QStringLiteral("1"), QString()));
    QVERIFY(!m_model->add(QStringLiteral("a.test"), QStringLiteral("a b"), QStringLiteral("1"), QString()));
    QVERIFY(!m_model->add(QStringLiteral("a.test"), QStringLiteral("a=b"), QStringLiteral("1"), QString()));
    QVERIFY(!m_model->add(QStringLiteral("a.test"), QStringLiteral("a"), QStringLiteral("1; b=2"), QString()));
    QVERIFY(!m_model->add(QStringLiteral("a.test"), QStringLiteral("a"), QStringLiteral("1\nb"), QString()));
    QCOMPARE(m_model->rowCount(), 0);
    QVERIFY(m_model->add(QStringLiteral("a.test"), QStringLiteral("a"), QStringLiteral("1"), QString()));
    QVERIFY(!m_model->setValue(0, QStringLiteral("x; y=z")));
    QCOMPARE(m_model->index(0).data(CookieModel::ValueRole).toString(), QStringLiteral("1"));
}

void CookieModelTest::enabledFollowsTheWorkspace()
{
    QSignalSpy changed(m_model.get(), &CookieModel::enabledChanged);
    QVERIFY(m_model->property("enabled").toBool());

    m_model->setProperty("enabled", false);

    QCOMPARE(changed.count(), 1);
    QVERIFY(!m_manager->cookiesEnabled());
    QVERIFY(m_manager->dirty());
}

QTEST_GUILESS_MAIN(CookieModelTest)

#include "tst_cookiemodel.moc"
