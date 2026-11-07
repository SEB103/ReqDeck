// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/collectiontree.h"

#include <algorithm>
#include <utility>

namespace {
/*!
 * \internal
 * \brief Longest name made from a request.
 */
constexpr qsizetype kMaxDisplayNameChars = 120;

/*!
 * \internal
 * \brief Returns the list directly holding the node with \a id, or null.
 */
QList<CollectionNode> *listHolding(QList<CollectionNode> &nodes, const QString &id)
{
    for (CollectionNode &node : nodes) {
        if (node.id == id)
            return &nodes;
        if (QList<CollectionNode> *list = listHolding(node.children, id))
            return list;
    }
    return nullptr;
}

/*!
 * \internal
 * \brief Returns the number of levels \a node occupies, 1 for a node without children.
 */
int subtreeHeight(const CollectionNode &node)
{
    int height = 0;
    for (const CollectionNode &child : node.children)
        height = std::max(height, subtreeHeight(child));
    return height + 1;
}

/*!
 * \internal
 * \brief Gives \a node and everything below it new ids.
 */
void assignFreshIds(CollectionNode &node)
{
    node.id = newItemId();
    for (CollectionNode &child : node.children)
        assignFreshIds(child);
}
} // namespace

/*!
 * \brief Returns a name for \a request made from another source.
 * \param request The request.
 * \return "METHOD url" on one line, or only the method without a URL.
 */
QString requestDisplayName(const RequestSpec &request)
{
    QString name = QString::fromLatin1(httpMethodVerb(request.method));
    const QString url = request.url.simplified();
    if (!url.isEmpty())
        name += u' ' + url;
    return name.left(kMaxDisplayNameChars);
}

/*!
 * \brief Returns the node with \a id below \a nodes.
 * \param nodes The top level of the tree.
 * \param id The node to find.
 * \return The node, or null when it does not exist.
 */
const CollectionNode *findNode(const QList<CollectionNode> &nodes, const QString &id)
{
    for (const CollectionNode &node : nodes) {
        if (node.id == id)
            return &node;
        if (const CollectionNode *child = findNode(node.children, id))
            return child;
    }
    return nullptr;
}

/*!
 * \brief Returns the node with \a id below \a nodes for editing.
 * \param nodes The top level of the tree.
 * \param id The node to find.
 * \return The node, or null when it does not exist.
 */
CollectionNode *findNode(QList<CollectionNode> &nodes, const QString &id)
{
    for (CollectionNode &node : nodes) {
        if (node.id == id)
            return &node;
        if (CollectionNode *child = findNode(node.children, id))
            return child;
    }
    return nullptr;
}

/*!
 * \brief Returns the nesting level of the node with \a id.
 * \param nodes The top level of the tree.
 * \param id The node to look up.
 * \return The level, 1 at the top; 0 when the node does not exist.
 */
int nodeDepth(const QList<CollectionNode> &nodes, const QString &id)
{
    for (const CollectionNode &node : nodes) {
        if (node.id == id)
            return 1;
        if (const int depth = nodeDepth(node.children, id); depth > 0)
            return depth + 1;
    }
    return 0;
}

/*!
 * \brief Appends \a node to a folder or to the top level.
 *
 * The whole subtree of \a node has to fit: the loader rejects anything nested
 * deeper than kMaxCollectionDepth, so a deeper tree could be saved but never
 * opened again.
 * \param nodes The top level of the tree.
 * \param parentId The folder, or an empty string for the top level.
 * \param node The node to add.
 * \return False when nothing was added.
 */
bool insertNode(QList<CollectionNode> &nodes, const QString &parentId, CollectionNode node)
{
    if (parentId.isEmpty()) {
        if (subtreeHeight(node) > kMaxCollectionDepth)
            return false;
        nodes.append(std::move(node));
        return true;
    }

    CollectionNode *parent = findNode(nodes, parentId);
    if (!parent || parent->kind != CollectionNode::Kind::Folder)
        return false;
    if (nodeDepth(nodes, parentId) + subtreeHeight(node) > kMaxCollectionDepth)
        return false;
    parent->children.append(std::move(node));
    return true;
}

/*!
 * \brief Removes the node with \a id and everything below it.
 * \param nodes The top level of the tree.
 * \param id The node to remove.
 * \return False when the node does not exist.
 */
bool removeNode(QList<CollectionNode> &nodes, const QString &id)
{
    QList<CollectionNode> *list = listHolding(nodes, id);
    if (!list)
        return false;
    list->removeIf([&id](const CollectionNode &node) { return node.id == id; });
    return true;
}

/*!
 * \brief Places a copy of the node with \a id right after it.
 * \param nodes The top level of the tree.
 * \param id The node to copy.
 * \return The id of the copy, or std::nullopt when the node does not exist.
 */
std::optional<QString> duplicateNode(QList<CollectionNode> &nodes, const QString &id)
{
    QList<CollectionNode> *list = listHolding(nodes, id);
    if (!list)
        return std::nullopt;

    const auto source = std::find_if(list->cbegin(), list->cend(),
                                     [&id](const CollectionNode &node) { return node.id == id; });
    const qsizetype row = source - list->cbegin();

    CollectionNode copy = *source;
    assignFreshIds(copy);
    copy.name += QStringLiteral(" copy");
    const QString copyId = copy.id;
    list->insert(row + 1, std::move(copy));
    return copyId;
}

/*!
 * \brief Sets the name of the node with \a id.
 * \param nodes The top level of the tree.
 * \param id The node to rename.
 * \param name The new name.
 * \return False when the node does not exist.
 */
bool renameNode(QList<CollectionNode> &nodes, const QString &id, const QString &name)
{
    CollectionNode *node = findNode(nodes, id);
    if (!node)
        return false;
    node->name = name;
    return true;
}

/*!
 * \brief Replaces the request of the request node with \a id.
 * \param nodes The top level of the tree.
 * \param id The node to update.
 * \param request The new request.
 * \return False for a folder or when the node does not exist.
 */
bool setNodeRequest(QList<CollectionNode> &nodes, const QString &id, const RequestSpec &request)
{
    CollectionNode *node = findNode(nodes, id);
    if (!node || node->kind != CollectionNode::Kind::Request)
        return false;
    node->request = request;
    return true;
}
