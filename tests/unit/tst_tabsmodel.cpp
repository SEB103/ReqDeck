// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemModelTester>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "core/workspace/workspaceserializer.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/tabsmodel.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \internal
 * \brief Returns a request node \a id named after it.
 */
static CollectionNode requestNode(const QString &id, HttpMethod method = HttpMethod::Get)
{
    CollectionNode node;
    node.id = id;
    node.kind = CollectionNode::Kind::Request;
    node.name = id.toUpper();
    node.request.method = method;
    node.request.url = QStringLiteral("https://h.test/") + id;
    return node;
}

/*! Verifies the open request tabs. */
class TabsModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Opens a workspace with requests r1 to r5 and a folder f. */
    void init();

    /*! Drops the model and the workspace. */
    void cleanup();

    /*! Verifies that previews replace the one preview tab, and an open request goes to its tab. */
    void previewTabIsReplaced();

    /*! Verifies that keeping or opening makes a tab stay, and the next preview opens a new one. */
    void keptTabsStay();

    /*! Verifies which tab is current after closing, and close others. */
    void closingChoosesTheNextTab();

    /*! Verifies that tabs of removed requests close and renames show. */
    void followsTheCollection();

    /*! Verifies moving and cycling. */
    void movesAndCycles();

    /*! Verifies that a running tab is not replaced by a preview nor closed by the limit. */
    void busyTabsAreKept();

    /*! Verifies the limit on open tabs. */
    void limitsTheTabs();

    /*! Verifies restore: order, preview, current, and unknown ids left out. */
    void tabsAreRestored();

    /*! Verifies that folders and unknown ids open no tab. */
    void ignoresFoldersAndUnknownIds();

    /*! Verifies that the tabs of a workspace are kept in the settings and come back with it. */
    void tabsComeBackWithTheirWorkspace();

    /*! Verifies that hidden tabs leave one preview tab, and the choice is kept. */
    void hiddenTabsKeepOnePreview();

    /*! Verifies that switching workspaces through the manager keeps each one's tabs. */
    void workspaceSwitchKeepsSavedTabs();

    /*! Verifies that closing a tab before the current one saves the right current tab. */
    void closingBeforeCurrentSavesCurrent();

    /*! Verifies that hidden tabs reuse the one tab even while it runs, and restore one tab. */
    void hiddenTabsStayOne();

    /*! Verifies that a method changed in the request shows on its tab. */
    void methodChangeShowsOnTab();

private:
    /*! Directory of the workspace. */
    std::unique_ptr<QTemporaryDir> m_dir;

    /*! Real workspace manager. */
    std::unique_ptr<WorkspaceManager> m_manager;

    /*! Real collection model. */
    std::unique_ptr<CollectionTreeModel> m_collection;

    /*! Model under test. */
    std::unique_ptr<TabsModel> m_tabs;

    /*! Consistency checks on the model under test. */
    std::unique_ptr<QAbstractItemModelTester> m_tester;
};

void TabsModelTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    m_manager = std::make_unique<WorkspaceManager>();
    m_manager->setSecretsDir(m_dir->filePath(QStringLiteral("secrets")));
    m_collection = std::make_unique<CollectionTreeModel>(m_manager.get());
    m_tabs = std::make_unique<TabsModel>(m_collection.get());
    m_tester = std::make_unique<QAbstractItemModelTester>(
        m_tabs.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);

    WorkspaceData data;
    data.id = newItemId();
    data.displayName = QStringLiteral("Tabs");
    CollectionNode folder;
    folder.id = QStringLiteral("f");
    folder.kind = CollectionNode::Kind::Folder;
    folder.name = QStringLiteral("F");
    folder.children = {requestNode(QStringLiteral("r4"), HttpMethod::Post), requestNode(QStringLiteral("r5"))};
    data.collection = {requestNode(QStringLiteral("r1")), requestNode(QStringLiteral("r2")),
                       requestNode(QStringLiteral("r3")), folder};
    const QString path = m_dir->filePath(QStringLiteral("tabs.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(path, data));
    QVERIFY(m_manager->openWorkspace(path));
}

void TabsModelTest::cleanup()
{
    m_tester.reset();
    m_tabs.reset();
    m_collection.reset();
    m_manager.reset();
    m_dir.reset();
}

void TabsModelTest::previewTabIsReplaced()
{
    QSignalSpy current(m_tabs.get(), &TabsModel::currentChanged);
    m_tabs->preview(QStringLiteral("r1"));
    m_tabs->preview(QStringLiteral("r2"));
    m_tabs->preview(QStringLiteral("r3"));

    QCOMPARE(m_tabs->rowCount(), 1);
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r3"));
    QVERIFY(m_tabs->index(0).data(TabsModel::PreviewRole).toBool());
    QCOMPARE(m_tabs->index(0).data(TabsModel::NameRole).toString(), QStringLiteral("R3"));
    QCOMPARE(current.count(), 3);

    m_tabs->open(QStringLiteral("r1"));
    m_tabs->preview(QStringLiteral("r1"));
    QCOMPARE(m_tabs->rowCount(), 2);
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r1"));
    QVERIFY(!m_tabs->index(1).data(TabsModel::PreviewRole).toBool());
}

void TabsModelTest::keptTabsStay()
{
    m_tabs->preview(QStringLiteral("r1"));
    m_tabs->keepCurrent();
    m_tabs->preview(QStringLiteral("r2"));
    m_tabs->open(QStringLiteral("r3"));
    m_tabs->open(QStringLiteral("r2"));

    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r1"), QStringLiteral("r2"), QStringLiteral("r3")}));
    QCOMPARE(m_tabs->previewRequestId(), QString());
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r2"));
    QCOMPARE(m_tabs->index(0).data(TabsModel::MethodRole).toString(), QStringLiteral("GET"));
}

void TabsModelTest::closingChoosesTheNextTab()
{
    QSignalSpy closed(m_tabs.get(), &TabsModel::tabClosed);
    for (const char *id : {"r1", "r2", "r3"})
        m_tabs->open(QString::fromLatin1(id));
    m_tabs->setCurrentIndex(1);

    m_tabs->close(1);
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r3"));
    m_tabs->close(1);
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r1"));
    QCOMPARE(closed.count(), 2);

    m_tabs->open(QStringLiteral("r2"));
    m_tabs->open(QStringLiteral("r3"));
    m_tabs->setCurrentIndex(2);
    m_tabs->close(0);
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r3"));
    m_tabs->closeOthers(0);
    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r2")}));
    QCOMPARE(m_tabs->currentIndex(), 0);
    m_tabs->close(0);
    QCOMPARE(m_tabs->currentIndex(), -1);
    QCOMPARE(m_tabs->currentRequestId(), QString());
}

void TabsModelTest::followsTheCollection()
{
    m_tabs->open(QStringLiteral("r1"));
    m_tabs->open(QStringLiteral("r4"));
    m_tabs->open(QStringLiteral("r5"));
    m_tabs->setCurrentIndex(1);
    QSignalSpy current(m_tabs.get(), &TabsModel::currentChanged);

    QVERIFY(m_collection->remove(QStringLiteral("f")));

    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r1")}));
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r1"));
    QVERIFY(current.count() > 0);

    QVERIFY(m_collection->rename(QStringLiteral("r1"), QStringLiteral("Renamed")));
    QCOMPARE(m_tabs->index(0).data(TabsModel::NameRole).toString(), QStringLiteral("Renamed"));
}

void TabsModelTest::movesAndCycles()
{
    for (const char *id : {"r1", "r2", "r3"})
        m_tabs->open(QString::fromLatin1(id));
    m_tabs->setCurrentIndex(0);

    m_tabs->move(0, 2);
    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r2"), QStringLiteral("r3"), QStringLiteral("r1")}));
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r1"));

    m_tabs->cycle(1);
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r2"));
    m_tabs->cycle(-1);
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r1"));
}

void TabsModelTest::busyTabsAreKept()
{
    m_tabs->preview(QStringLiteral("r1"));
    const int tab = m_tabs->currentTabId();
    m_tabs->setBusy(tab, true);
    QVERIFY(m_tabs->index(0).data(TabsModel::BusyRole).toBool());

    m_tabs->preview(QStringLiteral("r2"));

    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r1"), QStringLiteral("r2")}));
    m_tabs->setBusy(tab, false);
    QVERIFY(!m_tabs->index(0).data(TabsModel::BusyRole).toBool());
}

void TabsModelTest::limitsTheTabs()
{
    WorkspaceData data = m_manager->workspaceData();
    for (int i = 0; i < TabsModel::kMaxTabs + 2; ++i)
        data.collection.append(requestNode(QStringLiteral("x%1").arg(i)));
    m_manager->updateWorkspaceData(data);
    m_tabs->open(QStringLiteral("x0"));
    m_tabs->setBusy(m_tabs->currentTabId(), true);
    for (int i = 1; i < TabsModel::kMaxTabs + 2; ++i)
        m_tabs->open(QStringLiteral("x%1").arg(i));

    QCOMPARE(m_tabs->rowCount(), TabsModel::kMaxTabs);
    QVERIFY(m_tabs->requestIds().contains(QStringLiteral("x0")));
    QVERIFY(!m_tabs->requestIds().contains(QStringLiteral("x1")));
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("x%1").arg(TabsModel::kMaxTabs + 1));
}

void TabsModelTest::tabsAreRestored()
{
    m_tabs->restore({QStringLiteral("r3"), QStringLiteral("gone"), QStringLiteral("r1"), QStringLiteral("r3"),
                     QStringLiteral("r4")},
                    QStringLiteral("r4"), QStringLiteral("r1"));

    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r3"), QStringLiteral("r1"), QStringLiteral("r4")}));
    QCOMPARE(m_tabs->previewRequestId(), QStringLiteral("r4"));
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r1"));

    m_tabs->restore({QStringLiteral("r2")}, QString(), QStringLiteral("gone"));
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r2"));
    m_tabs->restore({}, QString(), QString());
    QCOMPARE(m_tabs->currentIndex(), -1);
}

void TabsModelTest::ignoresFoldersAndUnknownIds()
{
    m_tabs->preview(QStringLiteral("f"));
    m_tabs->open(QStringLiteral("nothing"));
    QCOMPARE(m_tabs->rowCount(), 0);
    QCOMPARE(m_tabs->currentIndex(), -1);
}

void TabsModelTest::tabsComeBackWithTheirWorkspace()
{
    QSettings settings(m_dir->filePath(QStringLiteral("ui.ini")), QSettings::IniFormat);
    m_tabs->setSettings(&settings);
    const QString id = m_manager->workspaceData().id;
    m_tabs->attachWorkspace(id);
    m_tabs->open(QStringLiteral("r1"));
    m_tabs->open(QStringLiteral("r3"));
    m_tabs->preview(QStringLiteral("r4"));
    m_tabs->setCurrentIndex(1);
    QVERIFY(!m_manager->dirty());

    m_tabs->attachWorkspace(QStringLiteral("another"));
    QCOMPARE(m_tabs->rowCount(), 0);
    m_tabs->attachWorkspace(id);

    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r1"), QStringLiteral("r3"), QStringLiteral("r4")}));
    QCOMPARE(m_tabs->previewRequestId(), QStringLiteral("r4"));
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r3"));
}

void TabsModelTest::hiddenTabsKeepOnePreview()
{
    QSettings settings(m_dir->filePath(QStringLiteral("ui.ini")), QSettings::IniFormat);
    m_tabs->setSettings(&settings);
    m_tabs->open(QStringLiteral("r1"));
    m_tabs->open(QStringLiteral("r2"));
    QSignalSpy changed(m_tabs.get(), &TabsModel::showTabsChanged);

    m_tabs->setShowTabs(false);

    QCOMPARE(changed.count(), 1);
    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r2")}));
    QCOMPARE(m_tabs->previewRequestId(), QStringLiteral("r2"));
    m_tabs->open(QStringLiteral("r3"));
    m_tabs->keepCurrent();
    m_tabs->preview(QStringLiteral("r1"));
    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r1")}));

    TabsModel other(m_collection.get());
    other.setSettings(&settings);
    QVERIFY(!other.showTabs());
}

void TabsModelTest::workspaceSwitchKeepsSavedTabs()
{
    QSettings settings(m_dir->filePath(QStringLiteral("ui.ini")), QSettings::IniFormat);
    m_tabs->setSettings(&settings);
    m_tabs->setWorkspaceIdSource([this]() { return m_manager->workspaceData().id; });
    connect(m_manager.get(), &WorkspaceManager::workspaceReplaced, m_tabs.get(),
            [this]() { m_tabs->attachWorkspace(m_manager->workspaceData().id); });
    const QString first = m_manager->activeWorkspacePath();
    m_tabs->attachWorkspace(m_manager->workspaceData().id);
    m_tabs->open(QStringLiteral("r1"));
    m_tabs->open(QStringLiteral("r2"));

    WorkspaceData other;
    other.id = newItemId();
    other.displayName = QStringLiteral("Other");
    other.collection = {requestNode(QStringLiteral("o1"))};
    const QString second = m_dir->filePath(QStringLiteral("other.reqdeck"));
    QVERIFY(WorkspaceSerializer::save(second, other));
    QVERIFY(m_manager->openWorkspace(second));
    QCOMPARE(m_tabs->rowCount(), 0);
    m_tabs->open(QStringLiteral("o1"));

    QVERIFY(m_manager->openWorkspace(first));
    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r1"), QStringLiteral("r2")}));
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r2"));

    m_manager->closeWorkspace();
    QCOMPARE(m_tabs->rowCount(), 0);
    QVERIFY(m_manager->openWorkspace(second));
    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("o1")}));
}

void TabsModelTest::closingBeforeCurrentSavesCurrent()
{
    QSettings settings(m_dir->filePath(QStringLiteral("ui.ini")), QSettings::IniFormat);
    m_tabs->setSettings(&settings);
    const QString id = m_manager->workspaceData().id;
    m_tabs->attachWorkspace(id);
    for (const char *request : {"r1", "r2", "r3"})
        m_tabs->open(QString::fromLatin1(request));
    QSignalSpy current(m_tabs.get(), &TabsModel::currentChanged);

    m_tabs->close(0);

    QVERIFY(current.count() > 0);
    QCOMPARE(m_tabs->currentIndex(), 1);
    QCOMPARE(settings.value(QStringLiteral("Tabs/%1/current").arg(id)).toString(), QStringLiteral("r3"));
    m_tabs->move(1, 0);
    QCOMPARE(m_tabs->currentIndex(), 0);
    QCOMPARE(m_tabs->currentRequestId(), QStringLiteral("r3"));
}

void TabsModelTest::hiddenTabsStayOne()
{
    m_tabs->setShowTabs(false);
    m_tabs->preview(QStringLiteral("r1"));
    m_tabs->setBusy(m_tabs->currentTabId(), true);
    m_tabs->preview(QStringLiteral("r2"));
    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r2")}));

    m_tabs->restore({QStringLiteral("r1"), QStringLiteral("r2"), QStringLiteral("r3")}, QString(), QStringLiteral("r2"));
    QCOMPARE(m_tabs->requestIds(), QStringList({QStringLiteral("r2")}));
    QCOMPARE(m_tabs->previewRequestId(), QStringLiteral("r2"));
}

void TabsModelTest::methodChangeShowsOnTab()
{
    m_tabs->open(QStringLiteral("r1"));
    QSignalSpy changed(m_tabs.get(), &QAbstractItemModel::dataChanged);
    RequestSpec request = *m_collection->request(QStringLiteral("r1"));
    request.method = HttpMethod::Delete;

    QVERIFY(m_collection->updateRequest(QStringLiteral("r1"), request));

    QVERIFY(changed.count() > 0);
    QCOMPARE(m_tabs->index(0).data(TabsModel::MethodRole).toString(), QStringLiteral("DELETE"));
}

QTEST_GUILESS_MAIN(TabsModelTest)

#include "tst_tabsmodel.moc"
