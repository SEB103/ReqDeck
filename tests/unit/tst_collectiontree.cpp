// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSet>
#include <QtTest>

#include "core/workspace/collectiontree.h"

/*!
 * \internal
 * \brief Returns a folder with \a id and \a children.
 */
static CollectionNode folder(const QString &id, const QList<CollectionNode> &children = {})
{
    CollectionNode node;
    node.id = id;
    node.name = id;
    node.children = children;
    return node;
}

/*!
 * \internal
 * \brief Returns a request node with \a id.
 */
static CollectionNode request(const QString &id)
{
    CollectionNode node;
    node.kind = CollectionNode::Kind::Request;
    node.id = id;
    node.name = id;
    node.request.url = QStringLiteral("http://h/") + id;
    return node;
}

/*!
 * \internal
 * \brief Returns the tree f1(f2(r1, r2), r3) followed by r4.
 */
static QList<CollectionNode> makeTree()
{
    return {folder(QStringLiteral("f1"),
                   {folder(QStringLiteral("f2"),
                           {request(QStringLiteral("r1")), request(QStringLiteral("r2"))}),
                    request(QStringLiteral("r3"))}),
            request(QStringLiteral("r4"))};
}

/*!
 * \internal
 * \brief Adds every id below \a nodes to \a ids and counts them.
 */
static int collectIds(const QList<CollectionNode> &nodes, QSet<QString> &ids)
{
    int count = 0;
    for (const CollectionNode &node : nodes) {
        ids.insert(node.id);
        count += 1 + collectIds(node.children, ids);
    }
    return count;
}

/*! Verifies the pure operations on a collection tree. */
class CollectionTreeTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that a node is found at any depth. */
    void findsNestedNode();

    /*! Verifies the result for an unknown id. */
    void findReturnsNullForMissingId();

    /*! Verifies that depth counts from one and is zero for an unknown id. */
    void depthCountsFromOne();

    /*! Verifies that an empty parent id appends at the top level. */
    void insertAppendsAtTopLevel();

    /*! Verifies insertion into a nested folder. */
    void insertAppendsInsideFolder();

    /*! Verifies that a request cannot have children. */
    void insertRefusedForRequestParent();

    /*! Verifies the refusal for an unknown parent. */
    void insertRefusedForMissingParent();

    /*! Verifies that nesting stops at the depth the loader accepts. */
    void insertRefusedBeyondDepthLimit();

    /*! Verifies that a nested node is removed with its children. */
    void removeDeletesNestedNode();

    /*! Verifies the result for an unknown id. */
    void removeMissingIdReturnsFalse();

    /*! Verifies the position and name of a copy. */
    void duplicatePlacesCopyAfterSource();

    /*! Verifies that a copied subtree shares no id with its source. */
    void duplicateAssignsFreshIdsInSubtree();

    /*! Verifies that renaming changes nothing else. */
    void renameChangesOnlyName();

    /*! Verifies that the request of a request node is replaced. */
    void setRequestUpdatesRequestNode();

    /*! Verifies that a folder has no request to replace. */
    void setRequestRefusedForFolder();
};

void CollectionTreeTest::findsNestedNode()
{
    const QList<CollectionNode> tree = makeTree();
    const CollectionNode *node = findNode(tree, QStringLiteral("r2"));
    QVERIFY(node);
    QCOMPARE(node->id, QStringLiteral("r2"));
}

void CollectionTreeTest::findReturnsNullForMissingId()
{
    const QList<CollectionNode> tree = makeTree();
    QVERIFY(!findNode(tree, QStringLiteral("nope")));
}

void CollectionTreeTest::depthCountsFromOne()
{
    const QList<CollectionNode> tree = makeTree();
    QCOMPARE(nodeDepth(tree, QStringLiteral("f1")), 1);
    QCOMPARE(nodeDepth(tree, QStringLiteral("f2")), 2);
    QCOMPARE(nodeDepth(tree, QStringLiteral("r1")), 3);
    QCOMPARE(nodeDepth(tree, QStringLiteral("nope")), 0);
}

void CollectionTreeTest::insertAppendsAtTopLevel()
{
    QList<CollectionNode> tree = makeTree();
    QVERIFY(insertNode(tree, QString(), request(QStringLiteral("new"))));
    QCOMPARE(tree.size(), 3);
    QCOMPARE(tree.last().id, QStringLiteral("new"));
}

void CollectionTreeTest::insertAppendsInsideFolder()
{
    QList<CollectionNode> tree = makeTree();
    QVERIFY(insertNode(tree, QStringLiteral("f2"), request(QStringLiteral("new"))));
    const CollectionNode *parent = findNode(tree, QStringLiteral("f2"));
    QCOMPARE(parent->children.size(), 3);
    QCOMPARE(parent->children.last().id, QStringLiteral("new"));
}

void CollectionTreeTest::insertRefusedForRequestParent()
{
    QList<CollectionNode> tree = makeTree();
    const QList<CollectionNode> before = tree;
    QVERIFY(!insertNode(tree, QStringLiteral("r1"), request(QStringLiteral("new"))));
    QVERIFY(tree == before);
}

void CollectionTreeTest::insertRefusedForMissingParent()
{
    QList<CollectionNode> tree = makeTree();
    QVERIFY(!insertNode(tree, QStringLiteral("nope"), request(QStringLiteral("new"))));
}

void CollectionTreeTest::insertRefusedBeyondDepthLimit()
{
    QList<CollectionNode> tree {folder(QStringLiteral("level1"))};
    for (int level = 2; level <= kMaxCollectionDepth; ++level) {
        QVERIFY(insertNode(tree, QStringLiteral("level%1").arg(level - 1),
                           folder(QStringLiteral("level%1").arg(level))));
    }
    QCOMPARE(nodeDepth(tree, QStringLiteral("level64")), 64);
    QVERIFY(!insertNode(tree, QStringLiteral("level64"), request(QStringLiteral("too-deep"))));
}

void CollectionTreeTest::removeDeletesNestedNode()
{
    QList<CollectionNode> tree = makeTree();
    QVERIFY(removeNode(tree, QStringLiteral("f2")));
    QVERIFY(!findNode(tree, QStringLiteral("f2")));
    QVERIFY(!findNode(tree, QStringLiteral("r1")));
    QVERIFY(findNode(tree, QStringLiteral("r3")));
}

void CollectionTreeTest::removeMissingIdReturnsFalse()
{
    QList<CollectionNode> tree = makeTree();
    QVERIFY(!removeNode(tree, QStringLiteral("nope")));
}

void CollectionTreeTest::duplicatePlacesCopyAfterSource()
{
    QList<CollectionNode> tree = makeTree();
    const std::optional<QString> id = duplicateNode(tree, QStringLiteral("r1"));
    QVERIFY(id);

    const QList<CollectionNode> &children = findNode(tree, QStringLiteral("f2"))->children;
    QCOMPARE(children.size(), 3);
    QCOMPARE(children.at(0).id, QStringLiteral("r1"));
    QCOMPARE(children.at(1).id, *id);
    QCOMPARE(children.at(1).name, QStringLiteral("r1 copy"));
    QCOMPARE(children.at(1).request, children.at(0).request);
    QCOMPARE(children.at(2).id, QStringLiteral("r2"));
}

void CollectionTreeTest::duplicateAssignsFreshIdsInSubtree()
{
    QList<CollectionNode> tree = makeTree();
    QVERIFY(duplicateNode(tree, QStringLiteral("f2")));

    QSet<QString> ids;
    const int count = collectIds(tree, ids);
    QCOMPARE(count, 9);
    QCOMPARE(ids.size(), count);
}

void CollectionTreeTest::renameChangesOnlyName()
{
    QList<CollectionNode> tree = makeTree();
    const RequestSpec request = findNode(tree, QStringLiteral("r1"))->request;
    QVERIFY(renameNode(tree, QStringLiteral("r1"), QStringLiteral("Renamed")));
    QCOMPARE(findNode(tree, QStringLiteral("r1"))->name, QStringLiteral("Renamed"));
    QCOMPARE(findNode(tree, QStringLiteral("r1"))->request, request);
    QVERIFY(!renameNode(tree, QStringLiteral("nope"), QStringLiteral("x")));
}

void CollectionTreeTest::setRequestUpdatesRequestNode()
{
    QList<CollectionNode> tree = makeTree();
    RequestSpec spec;
    spec.url = QStringLiteral("http://changed/");
    QVERIFY(setNodeRequest(tree, QStringLiteral("r3"), spec));
    QCOMPARE(findNode(tree, QStringLiteral("r3"))->request, spec);
}

void CollectionTreeTest::setRequestRefusedForFolder()
{
    QList<CollectionNode> tree = makeTree();
    QVERIFY(!setNodeRequest(tree, QStringLiteral("f1"), RequestSpec {}));
    QVERIFY(!setNodeRequest(tree, QStringLiteral("nope"), RequestSpec {}));
}

QTEST_GUILESS_MAIN(CollectionTreeTest)

#include "tst_collectiontree.moc"
