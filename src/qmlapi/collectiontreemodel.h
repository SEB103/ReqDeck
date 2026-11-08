// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLLECTIONTREEMODEL_H
#define COLLECTIONTREEMODEL_H

#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <QString>

#include <memory>

#include "core/workspace/workspacedata.h"

class WorkspaceManager;

/**
 * Tree of the active workspace's collection, exposed to QML as \c cppCollection.
 *
 * All changes to the collection go through this model, which applies them to the
 * workspace and marks it dirty. Requests are addressed by their stable node id.
 */
class CollectionTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CollectionTreeModel)

public:
    /** Data roles offered in addition to Qt::DisplayRole. */
    enum Role {
        NodeIdRole = Qt::UserRole + 1, /**< Stable id of the node. */
        NameRole,                      /**< Name of the node. */
        IsFolderRole,                  /**< Whether the node is a folder. */
        MethodRole                     /**< Request verb; empty for folders. */
    };

    /** Creates a model over the active workspace of \a manager, which must outlive it. */
    explicit CollectionTreeModel(WorkspaceManager *manager, QObject *parent = nullptr);

    /** Destroys the model. */
    ~CollectionTreeModel() override;

    /** Returns the index of \a row and \a column below \a parent. */
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;

    /** Returns the parent of \a child. */
    QModelIndex parent(const QModelIndex &child) const override;

    /** Returns the number of children of \a parent. */
    int rowCount(const QModelIndex &parent = {}) const override;

    /** Returns 1: the tree has one column. */
    int columnCount(const QModelIndex &parent = {}) const override;

    /** Returns the value of \a role for \a index. */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** Returns the QML names of the roles. */
    QHash<int, QByteArray> roleNames() const override;

    /** Adds a request to the folder \a parentId or the top level; returns its id or "". */
    Q_INVOKABLE QString addRequest(const QString &parentId);

    /** Adds a folder to the folder \a parentId or the top level; returns its id or "". */
    Q_INVOKABLE QString addFolder(const QString &parentId);

    /** Adds a request holding \a request as last child of \a parentId; returns its id or "". */
    QString addRequestFrom(const QString &parentId, const RequestSpec &request, const QString &name);

    /** Sets the trimmed \a name of the node \a id; a blank name is refused. */
    Q_INVOKABLE bool rename(const QString &id, const QString &name);

    /** Removes the node \a id and everything below it. */
    Q_INVOKABLE bool remove(const QString &id);

    /** Places a copy of the node \a id after it; returns the id of the copy or "". */
    Q_INVOKABLE QString duplicate(const QString &id);

    /** Returns the index of the node \a id; invalid when it does not exist. */
    Q_INVOKABLE QModelIndex indexOfNode(const QString &id) const;

    /** Returns the name of the node \a id; empty when it does not exist. */
    Q_INVOKABLE QString nodeName(const QString &id) const;

    /** Returns whether \a id is a folder. */
    Q_INVOKABLE bool isFolder(const QString &id) const;

    /**
     * Returns the request to open first: the one the workspace setting "startRequest"
     * names, or else the first request in tree order; empty without requests.
     */
    Q_INVOKABLE QString startRequestId() const;

    /** Returns the folder a new item belongs to when the node \a id is selected. */
    Q_INVOKABLE QString parentForNewItem(const QString &id) const;

    /** Returns the request of the node \a id; null for folders and unknown ids. */
    const RequestSpec *request(const QString &id) const;

    /** Replaces the request of the node \a id and marks the workspace dirty. */
    bool updateRequest(const QString &id, const RequestSpec &request);

signals:
    /** Emitted after a node was added, removed, or renamed, and after a reset. */
    void collectionChanged();

private:
    /** Mirror of one node; the indices of the model point at these. */
    struct Item;

    /** Adds \a node below \a parentId as the last child. */
    QString addNode(const QString &parentId, CollectionNode node);

    /** Stores \a collection in the workspace and remembers it as mirrored. */
    void commit(const QList<CollectionNode> &collection);

    /** Rebuilds the mirror unless it already matches the workspace. */
    void syncWithWorkspace();

    /** Creates the mirror of \a node below \a parent and registers it by id. */
    std::unique_ptr<Item> buildItem(const CollectionNode &node, Item *parent);

    /** Removes \a item and everything below it from the id lookup. */
    void forget(const Item *item);

    /** Returns the row of \a item below its parent. */
    int rowOf(const Item *item) const;

    /** Returns the index of \a item; the invalid index for the root. */
    QModelIndex indexOf(const Item *item) const;

    /** Workspace the collection belongs to; not owned. */
    WorkspaceManager *m_manager {nullptr};

    /** Invisible root of the mirror. */
    std::unique_ptr<Item> m_root;

    /** Mirror items by node id. */
    QHash<QString, Item *> m_items;

    /** The collection the mirror was last built from or changed to. */
    QList<CollectionNode> m_mirrored;
};

#endif // COLLECTIONTREEMODEL_H
