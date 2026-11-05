// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "core/workspace/workspaceserializer.h"
#include "qmlapi/collectiontreemodel.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \internal
 * \brief Returns a request node named after \a id.
 */
static CollectionNode requestNode(const QString &id, HttpMethod method = HttpMethod::Get)
{
    CollectionNode node;
    node.kind = CollectionNode::Kind::Request;
    node.id = id;
    node.name = id;
    node.request.method = method;
    return node;
}

/*!
 * \internal
 * \brief Returns the collection f1(r1, f2(r2)) followed by r3.
 */
static QList<CollectionNode> makeCollection()
{
    CollectionNode inner;
    inner.id = QStringLiteral("f2");
    inner.name = QStringLiteral("f2");
    inner.children = {requestNode(QStringLiteral("r2"))};

    CollectionNode outer;
    outer.id = QStringLiteral("f1");
    outer.name = QStringLiteral("f1");
    outer.children = {requestNode(QStringLiteral("r1"), HttpMethod::Post), inner};

    return {outer, requestNode(QStringLiteral("r3"))};
}

/*! Verifies the collection tree model over a real workspace. */
class CollectionTreeModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Opens a workspace with a known collection for each test. */
    void init();

    /*! Releases the per-test objects. */
    void cleanup();

    /*! Verifies rows and parents of the nested collection. */
    void exposesNestedStructure();

    /*! Verifies the custom roles. */
    void rolesReportNameKindAndMethod();

    /*! Verifies that adding a request inserts a row and marks the workspace dirty. */
    void addRequestInsertsRowAndMarksDirty();

    /*! Verifies adding a folder inside a folder. */
    void addFolderInsideFolder();

    /*! Verifies that a request takes no children. */
    void addRequestUnderRequestRefused();

    /*! Verifies that folders stop at the depth the loader accepts. */
    void addFolderRefusedAtDepthLimit();

    /*! Verifies the change notification of a rename. */
    void renameEmitsDataChanged();

    /*! Verifies that a blank name is refused. */
    void renameRefusesEmptyName();

    /*! Verifies that removing a folder removes its subtree. */
    void removeDeletesSubtree();

    /*! Verifies the position of a duplicate. */
    void duplicateInsertsAfterSource();

    /*! Verifies that a changed method shows in the tree. */
    void updateRequestChangesMethodRole();

    /*! Verifies the lookups used by the rename and delete dialogs. */
    void nodeNameAndKindByIdentifier();

    /*! Verifies where a new item goes for a selected folder or request. */
    void parentForNewItemResolvesFolderAndRequest();

    /*! Verifies that opening another workspace resets the model. */
    void openingWorkspaceResetsModel();

    /*! Verifies that closing the workspace empties the model. */
    void closingWorkspaceEmptiesModel();

    /*! Verifies that saving keeps the model, and with it expansion and selection. */
    void saveDoesNotResetModel();

private:
    /*! Writes a workspace file holding \a collection; returns its path. */
    QString writeWorkspace(const QString &name, const QList<CollectionNode> &collection);

    /*! Directory of the per-test workspace files. */
    std::unique_ptr<QTemporaryDir> m_dir;

    /*! Workspace manager with the open test workspace. */
    std::unique_ptr<WorkspaceManager> m_manager;

    /*! Model under test. */
    std::unique_ptr<CollectionTreeModel> m_model;

    /*! Consistency checker attached to the model. */
    std::unique_ptr<QAbstractItemModelTester> m_tester;
};

QString CollectionTreeModelTest::writeWorkspace(const QString &name,
                                                const QList<CollectionNode> &collection)
{
    WorkspaceData data;
    data.id = newItemId();
    data.displayName = name;
    data.collection = collection;
    const QString path = m_dir->filePath(name + QStringLiteral(".reqdeck"));
    return WorkspaceSerializer::save(path, data) ? path : QString();
}

void CollectionTreeModelTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_manager = std::make_unique<WorkspaceManager>();
    m_manager->setSecretsDir(m_dir->filePath(QStringLiteral("secrets")));
    m_model = std::make_unique<CollectionTreeModel>(m_manager.get());
    m_tester = std::make_unique<QAbstractItemModelTester>(
        m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
    QVERIFY(m_manager->openWorkspace(writeWorkspace(QStringLiteral("Main"), makeCollection())));
}

void CollectionTreeModelTest::cleanup()
{
    m_tester.reset();
    m_model.reset();
    m_manager.reset();
    m_dir.reset();
}

void CollectionTreeModelTest::exposesNestedStructure()
{
    QCOMPARE(m_model->rowCount(), 2);
    const QModelIndex f1 = m_model->index(0, 0);
    QCOMPARE(m_model->rowCount(f1), 2);
    const QModelIndex f2 = m_model->index(1, 0, f1);
    QCOMPARE(m_model->parent(f2), f1);
    QCOMPARE(m_model->rowCount(f2), 1);
    QCOMPARE(m_model->indexOfNode(QStringLiteral("r2")), m_model->index(0, 0, f2));
    QVERIFY(!m_model->indexOfNode(QStringLiteral("nope")).isValid());
}

void CollectionTreeModelTest::rolesReportNameKindAndMethod()
{
    const QModelIndex folder = m_model->indexOfNode(QStringLiteral("f1"));
    QCOMPARE(folder.data(CollectionTreeModel::NodeIdRole).toString(), QStringLiteral("f1"));
    QCOMPARE(folder.data(CollectionTreeModel::NameRole).toString(), QStringLiteral("f1"));
    QCOMPARE(folder.data(Qt::DisplayRole).toString(), QStringLiteral("f1"));
    QVERIFY(folder.data(CollectionTreeModel::IsFolderRole).toBool());
    QVERIFY(folder.data(CollectionTreeModel::MethodRole).toString().isEmpty());

    const QModelIndex request = m_model->indexOfNode(QStringLiteral("r1"));
    QVERIFY(!request.data(CollectionTreeModel::IsFolderRole).toBool());
    QCOMPARE(request.data(CollectionTreeModel::MethodRole).toString(), QStringLiteral("POST"));

    const QHash<int, QByteArray> names = m_model->roleNames();
    QCOMPARE(names.value(CollectionTreeModel::NodeIdRole), QByteArray("nodeId"));
    QCOMPARE(names.value(CollectionTreeModel::IsFolderRole), QByteArray("isFolder"));
    QCOMPARE(names.value(CollectionTreeModel::MethodRole), QByteArray("method"));
}

void CollectionTreeModelTest::addRequestInsertsRowAndMarksDirty()
{
    QVERIFY(!m_manager->dirty());
    QSignalSpy changed(m_model.get(), &CollectionTreeModel::collectionChanged);

    const QString id = m_model->addRequest(QString());
    QVERIFY(!id.isEmpty());
    QCOMPARE(m_model->rowCount(), 3);
    QCOMPARE(m_model->indexOfNode(id).data(CollectionTreeModel::NameRole).toString(),
             QStringLiteral("New request"));
    QCOMPARE(m_model->indexOfNode(id).data(CollectionTreeModel::MethodRole).toString(),
             QStringLiteral("GET"));
    QVERIFY(m_manager->dirty());
    QVERIFY(m_model->request(id));
    QCOMPARE(changed.count(), 1);
}

void CollectionTreeModelTest::addFolderInsideFolder()
{
    const QString id = m_model->addFolder(QStringLiteral("f2"));
    QVERIFY(!id.isEmpty());
    const QModelIndex index = m_model->indexOfNode(id);
    QCOMPARE(index.data(CollectionTreeModel::NameRole).toString(), QStringLiteral("New folder"));
    QVERIFY(index.data(CollectionTreeModel::IsFolderRole).toBool());
    QCOMPARE(m_model->parent(index), m_model->indexOfNode(QStringLiteral("f2")));
    QVERIFY(!m_model->request(id));
}

void CollectionTreeModelTest::addRequestUnderRequestRefused()
{
    QVERIFY(m_model->addRequest(QStringLiteral("r1")).isEmpty());
    QVERIFY(!m_manager->dirty());
}

void CollectionTreeModelTest::addFolderRefusedAtDepthLimit()
{
    QString parent;
    for (int level = 1; level <= kMaxCollectionDepth; ++level) {
        parent = m_model->addFolder(parent);
        QVERIFY(!parent.isEmpty());
    }
    QVERIFY(m_model->addFolder(parent).isEmpty());
    QCOMPARE(m_model->rowCount(m_model->indexOfNode(parent)), 0);
}

void CollectionTreeModelTest::renameEmitsDataChanged()
{
    QSignalSpy spy(m_model.get(), &QAbstractItemModel::dataChanged);
    QVERIFY(m_model->rename(QStringLiteral("r1"), QStringLiteral("  Renamed  ")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->indexOfNode(QStringLiteral("r1")).data(CollectionTreeModel::NameRole).toString(),
             QStringLiteral("Renamed"));
    QVERIFY(m_manager->dirty());
}

void CollectionTreeModelTest::renameRefusesEmptyName()
{
    QVERIFY(!m_model->rename(QStringLiteral("r1"), QStringLiteral("   ")));
    QVERIFY(!m_manager->dirty());
}

void CollectionTreeModelTest::removeDeletesSubtree()
{
    QVERIFY(m_model->remove(QStringLiteral("f2")));
    QVERIFY(!m_model->indexOfNode(QStringLiteral("f2")).isValid());
    QVERIFY(!m_model->indexOfNode(QStringLiteral("r2")).isValid());
    QVERIFY(!m_model->request(QStringLiteral("r2")));
    QCOMPARE(m_model->rowCount(m_model->indexOfNode(QStringLiteral("f1"))), 1);
    QVERIFY(!m_model->remove(QStringLiteral("nope")));
}

void CollectionTreeModelTest::duplicateInsertsAfterSource()
{
    const QString id = m_model->duplicate(QStringLiteral("f2"));
    QVERIFY(!id.isEmpty());
    const QModelIndex copy = m_model->indexOfNode(id);
    QCOMPARE(copy.row(), m_model->indexOfNode(QStringLiteral("f2")).row() + 1);
    QCOMPARE(copy.data(CollectionTreeModel::NameRole).toString(), QStringLiteral("f2 copy"));
    QCOMPARE(m_model->rowCount(copy), 1);
    QVERIFY(m_model->duplicate(QStringLiteral("nope")).isEmpty());
}

void CollectionTreeModelTest::updateRequestChangesMethodRole()
{
    QSignalSpy spy(m_model.get(), &QAbstractItemModel::dataChanged);
    RequestSpec spec = *m_model->request(QStringLiteral("r3"));
    spec.method = HttpMethod::Delete;
    QVERIFY(m_model->updateRequest(QStringLiteral("r3"), spec));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->indexOfNode(QStringLiteral("r3")).data(CollectionTreeModel::MethodRole).toString(),
             QStringLiteral("DELETE"));
    QCOMPARE(m_model->request(QStringLiteral("r3"))->method, HttpMethod::Delete);
    QVERIFY(m_manager->dirty());
}

void CollectionTreeModelTest::nodeNameAndKindByIdentifier()
{
    QCOMPARE(m_model->nodeName(QStringLiteral("r1")), QStringLiteral("r1"));
    QVERIFY(m_model->isFolder(QStringLiteral("f1")));
    QVERIFY(!m_model->isFolder(QStringLiteral("r1")));
    QVERIFY(m_model->nodeName(QStringLiteral("nope")).isEmpty());
    QVERIFY(!m_model->isFolder(QStringLiteral("nope")));
}

void CollectionTreeModelTest::parentForNewItemResolvesFolderAndRequest()
{
    QCOMPARE(m_model->parentForNewItem(QStringLiteral("f1")), QStringLiteral("f1"));
    QCOMPARE(m_model->parentForNewItem(QStringLiteral("r2")), QStringLiteral("f2"));
    QCOMPARE(m_model->parentForNewItem(QStringLiteral("r3")), QString());
    QCOMPARE(m_model->parentForNewItem(QString()), QString());
}

void CollectionTreeModelTest::openingWorkspaceResetsModel()
{
    QSignalSpy spy(m_model.get(), &QAbstractItemModel::modelReset);
    const QString other = writeWorkspace(QStringLiteral("Other"), {requestNode(QStringLiteral("x"))});
    QVERIFY(m_manager->openWorkspace(other));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(m_model->rowCount(), 1);
    QVERIFY(m_model->indexOfNode(QStringLiteral("x")).isValid());
}

void CollectionTreeModelTest::closingWorkspaceEmptiesModel()
{
    m_manager->closeWorkspace();
    QCOMPARE(m_model->rowCount(), 0);
    QVERIFY(m_model->addRequest(QString()).isEmpty());
}

void CollectionTreeModelTest::saveDoesNotResetModel()
{
    QVERIFY(!m_model->addRequest(QStringLiteral("f1")).isEmpty());

    QSignalSpy spy(m_model.get(), &QAbstractItemModel::modelReset);
    QVERIFY(m_manager->saveWorkspace());
    QVERIFY(m_manager->saveWorkspaceAs(m_dir->filePath(QStringLiteral("Copy.reqdeck"))));
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(CollectionTreeModelTest)

#include "tst_collectiontreemodel.moc"
