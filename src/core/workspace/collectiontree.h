// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef COLLECTIONTREE_H
#define COLLECTIONTREE_H

#include <QList>
#include <QString>

#include <optional>

#include "core/workspace/workspacedata.h"

/** Returns the node with \a id below \a nodes, or null. */
const CollectionNode *findNode(const QList<CollectionNode> &nodes, const QString &id);

/** Returns the node with \a id below \a nodes for editing, or null. */
CollectionNode *findNode(QList<CollectionNode> &nodes, const QString &id);

/** Returns the nesting level of \a id, 1 at the top level; 0 when it does not exist. */
int nodeDepth(const QList<CollectionNode> &nodes, const QString &id);

/**
 * Appends \a node to the folder \a parentId, or to the top level when it is empty.
 * Fails when the parent is missing or a request, or beyond kMaxCollectionDepth.
 */
bool insertNode(QList<CollectionNode> &nodes, const QString &parentId, CollectionNode node);

/** Removes the node with \a id and everything below it. */
bool removeNode(QList<CollectionNode> &nodes, const QString &id);

/**
 * Places a copy of the node with \a id right after it, with new ids throughout
 * the subtree and "<name> copy" as name. Returns the id of the copy.
 */
std::optional<QString> duplicateNode(QList<CollectionNode> &nodes, const QString &id);

/** Sets the name of the node with \a id. */
bool renameNode(QList<CollectionNode> &nodes, const QString &id, const QString &name);

/** Replaces the request of the request node with \a id. */
bool setNodeRequest(QList<CollectionNode> &nodes, const QString &id, const RequestSpec &request);

#endif // COLLECTIONTREE_H
