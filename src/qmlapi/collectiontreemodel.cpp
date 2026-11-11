// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/collectiontreemodel.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "core/workspace/collectiontree.h"
#include "qmlapi/workspacemanager.h"

/*!
 * \internal
 * \brief Mirror of one collection node.
 *
 * Model indices must stay valid while the implicitly shared QList of the
 * workspace detaches, so they point at these items and never into the list.
 */
struct CollectionTreeModel::Item
{
    QString id;
    QString name;
    QString method;
    bool isFolder {true};
    Item *parent {nullptr};
    std::vector<std::unique_ptr<Item>> children;
};

/*!
 * \brief Creates a model over the active workspace of \a manager.
 *
 * WorkspaceManager reports every save as a change of the active workspace too,
 * so the model only rebuilds when the collection really differs: a reset would
 * collapse the tree and drop the selection each time the user saves.
 * \param manager The workspace manager.
 * \param parent The QObject parent.
 */
CollectionTreeModel::CollectionTreeModel(WorkspaceManager *manager, QObject *parent)
    : QAbstractItemModel(parent)
    , m_manager(manager)
    , m_root(std::make_unique<Item>())
{
    connect(m_manager, &WorkspaceManager::activeWorkspaceChanged, this,
            &CollectionTreeModel::syncWithWorkspace);
    syncWithWorkspace();
}

/*!
 * \brief Destroys the model.
 */
CollectionTreeModel::~CollectionTreeModel() = default;

/*!
 * \brief Returns the index of \a row and \a column below \a parent.
 */
QModelIndex CollectionTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    const Item *item = parent.isValid() ? static_cast<Item *>(parent.internalPointer()) : m_root.get();
    return createIndex(row, column, item->children.at(size_t(row)).get());
}

/*!
 * \brief Returns the parent of \a child.
 */
QModelIndex CollectionTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};
    return indexOf(static_cast<Item *>(child.internalPointer())->parent);
}

/*!
 * \brief Returns the number of children of \a parent.
 */
int CollectionTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    const Item *item = parent.isValid() ? static_cast<Item *>(parent.internalPointer()) : m_root.get();
    return int(item->children.size());
}

/*!
 * \brief Returns 1: the tree has one column.
 */
int CollectionTreeModel::columnCount(const QModelIndex &) const
{
    return 1;
}

/*!
 * \brief Returns the value of \a role for \a index.
 */
QVariant CollectionTreeModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};

    const Item *item = static_cast<Item *>(index.internalPointer());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return item->name;
    case NodeIdRole:
        return item->id;
    case IsFolderRole:
        return item->isFolder;
    case MethodRole:
        return item->method;
    default:
        return {};
    }
}

/*!
 * \brief Returns the QML names of the roles.
 */
QHash<int, QByteArray> CollectionTreeModel::roleNames() const
{
    return {{Qt::DisplayRole, QByteArrayLiteral("display")},
            {NodeIdRole, QByteArrayLiteral("nodeId")},
            {NameRole, QByteArrayLiteral("name")},
            {IsFolderRole, QByteArrayLiteral("isFolder")},
            {MethodRole, QByteArrayLiteral("method")}};
}

/*!
 * \brief Adds a new GET request as the last child of \a parentId.
 * \param parentId The folder, or an empty string for the top level.
 * \return The id of the request, or an empty string when it cannot go there.
 */
QString CollectionTreeModel::addRequest(const QString &parentId)
{
    CollectionNode node;
    node.kind = CollectionNode::Kind::Request;
    node.id = newItemId();
    node.name = tr("New request");
    return addNode(parentId, std::move(node));
}

/*!
 * \brief Adds a request made from another source as the last child of \a parentId.
 * \param parentId The folder, or an empty string for the top level.
 * \param request The content of the new request.
 * \param name The name of the new request.
 * \return The id of the request, or an empty string when it cannot go there.
 */
QString CollectionTreeModel::addRequestFrom(const QString &parentId, const RequestSpec &request,
                                            const QString &name)
{
    if (!m_manager->hasActiveWorkspace())
        return {};

    CollectionNode node;
    node.kind = CollectionNode::Kind::Request;
    node.id = newItemId();
    node.name = name;
    node.request = request;
    return addNode(parentId, std::move(node));
}

/*!
 * \brief Adds a new folder as the last child of \a parentId.
 * \param parentId The folder, or an empty string for the top level.
 * \return The id of the folder, or an empty string when it cannot go there.
 */
QString CollectionTreeModel::addFolder(const QString &parentId)
{
    CollectionNode node;
    node.id = newItemId();
    node.name = tr("New folder");
    return addNode(parentId, std::move(node));
}

/*!
 * \internal
 * \brief Adds \a node below \a parentId as the last child.
 * \return The id of the node, or an empty string when insertNode() refused it.
 */
/*!
 * \brief Adds an imported \a folder at the end of the top level.
 * \param folder The folder with its ids already assigned.
 * \return Its id, or an empty string without a workspace or when the tree refuses it.
 */
QString CollectionTreeModel::addImportedFolder(const CollectionNode &folder)
{
    return addNode(QString(), folder);
}

QString CollectionTreeModel::addNode(const QString &parentId, CollectionNode node)
{
    if (!m_manager->hasActiveWorkspace())
        return {};

    QList<CollectionNode> collection = m_manager->workspaceData().collection;
    const QString id = node.id;
    if (!insertNode(collection, parentId, node))
        return {};

    Item *parent = parentId.isEmpty() ? m_root.get() : m_items.value(parentId);
    const int row = int(parent->children.size());
    beginInsertRows(indexOf(parent), row, row);
    parent->children.push_back(buildItem(node, parent));
    commit(collection);
    endInsertRows();

    emit collectionChanged();
    return id;
}

/*!
 * \brief Sets the name of the node \a id.
 * \param id The node.
 * \param name The new name; surrounding whitespace is dropped.
 * \return False for a blank name or an unknown node.
 */
bool CollectionTreeModel::rename(const QString &id, const QString &name)
{
    const QString trimmed = name.trimmed();
    Item *item = m_items.value(id);
    if (!item || trimmed.isEmpty())
        return false;
    if (item->name == trimmed)
        return true;

    QList<CollectionNode> collection = m_manager->workspaceData().collection;
    if (!renameNode(collection, id, trimmed))
        return false;

    item->name = trimmed;
    commit(collection);
    const QModelIndex index = indexOf(item);
    emit dataChanged(index, index, {Qt::DisplayRole, NameRole});
    emit collectionChanged();
    return true;
}

/*!
 * \brief Removes the node \a id and everything below it.
 * \param id The node.
 * \return False for an unknown node.
 */
bool CollectionTreeModel::remove(const QString &id)
{
    Item *item = m_items.value(id);
    if (!item)
        return false;

    QList<CollectionNode> collection = m_manager->workspaceData().collection;
    if (!removeNode(collection, id))
        return false;

    Item *parent = item->parent;
    const int row = rowOf(item);
    beginRemoveRows(indexOf(parent), row, row);
    forget(item);
    parent->children.erase(parent->children.begin() + row);
    commit(collection);
    endRemoveRows();

    emit collectionChanged();
    return true;
}

/*!
 * \brief Places a copy of the node \a id right after it.
 * \param id The node.
 * \return The id of the copy, or an empty string for an unknown node.
 */
QString CollectionTreeModel::duplicate(const QString &id)
{
    Item *item = m_items.value(id);
    if (!item)
        return {};

    QList<CollectionNode> collection = m_manager->workspaceData().collection;
    const std::optional<QString> copyId = duplicateNode(collection, id);
    if (!copyId)
        return {};

    Item *parent = item->parent;
    const int row = rowOf(item) + 1;
    beginInsertRows(indexOf(parent), row, row);
    parent->children.insert(parent->children.begin() + row,
                            buildItem(*findNode(std::as_const(collection), *copyId), parent));
    commit(collection);
    endInsertRows();

    emit collectionChanged();
    return *copyId;
}

/*!
 * \brief Removes several nodes.
 *
 * A node inside another one of \a ids goes with it and is not removed twice.
 * \param ids The nodes; unknown ids are skipped.
 * \return The number of nodes removed, not counting what they held.
 */
int CollectionTreeModel::removeItems(const QStringList &ids)
{
    int removed = 0;
    for (const QString &id : outermost(ids)) {
        if (remove(id))
            ++removed;
    }
    return removed;
}

/*!
 * \brief Places a copy after each of several nodes.
 * \param ids The nodes; unknown ids and nodes inside another one are skipped.
 * \return The ids of the copies, in the order of \a ids.
 */
QStringList CollectionTreeModel::duplicateItems(const QStringList &ids)
{
    QStringList copies;
    for (const QString &id : outermost(ids)) {
        const QString copy = duplicate(id);
        if (!copy.isEmpty())
            copies.append(copy);
    }
    return copies;
}

/*!
 * \brief Sorts the children of a folder or the top level.
 *
 * Folders come first, then requests, each by name without regard to case; equal
 * names keep their order. The new order is stored; the tree keeps its expanded
 * folders, because the change is a layout change, not a reset.
 * \param folderId The folder, or an empty string for the top level.
 * \return False for a request or an unknown id.
 */
bool CollectionTreeModel::sortChildren(const QString &folderId)
{
    Item *parent = folderId.isEmpty() ? m_root.get() : m_items.value(folderId);
    if (!parent || !parent->isFolder)
        return false;

    QList<CollectionNode> collection = m_manager->workspaceData().collection;
    QList<CollectionNode> *nodes = &collection;
    if (!folderId.isEmpty()) {
        CollectionNode *folder = findNode(collection, folderId);
        if (!folder)
            return false;
        nodes = &folder->children;
    }
    const auto before = [](bool folderA, const QString &nameA, bool folderB, const QString &nameB) {
        if (folderA != folderB)
            return folderA;
        return QString::compare(nameA, nameB, Qt::CaseInsensitive) < 0;
    };
    std::stable_sort(nodes->begin(), nodes->end(), [&before](const CollectionNode &a, const CollectionNode &b) {
        return before(a.kind == CollectionNode::Kind::Folder, a.name, b.kind == CollectionNode::Kind::Folder, b.name);
    });

    const QPersistentModelIndex parentIndex(indexOf(parent));
    emit layoutAboutToBeChanged({parentIndex}, QAbstractItemModel::VerticalSortHint);
    const QModelIndexList persistent = persistentIndexList();
    std::stable_sort(parent->children.begin(), parent->children.end(),
                     [&before](const std::unique_ptr<Item> &a, const std::unique_ptr<Item> &b) {
                         return before(a->isFolder, a->name, b->isFolder, b->name);
                     });
    for (const QModelIndex &old : persistent) {
        const Item *item = static_cast<const Item *>(old.internalPointer());
        if (item && item->parent == parent)
            changePersistentIndex(old, createIndex(rowOf(item), old.column(), item));
    }
    commit(collection);
    emit layoutChanged({parentIndex}, QAbstractItemModel::VerticalSortHint);

    emit collectionChanged();
    return true;
}

/*!
 * \brief Returns every request in tree order, for finding one by name or URL.
 * \return Maps with \c id, \c name, \c method, and \c url, the URL with its
 *         variables unresolved.
 */
QVariantList CollectionTreeModel::requestList() const
{
    QVariantList list;
    const std::function<void(const QList<CollectionNode> &)> collect = [&](const QList<CollectionNode> &nodes) {
        for (const CollectionNode &node : nodes) {
            if (node.kind == CollectionNode::Kind::Request) {
                list.append(QVariantMap {{QStringLiteral("id"), node.id},
                                         {QStringLiteral("name"), node.name},
                                         {QStringLiteral("method"), QString::fromLatin1(httpMethodVerb(node.request.method))},
                                         {QStringLiteral("url"), node.request.url}});
            }
            collect(node.children);
        }
    };
    collect(m_manager->workspaceData().collection);
    return list;
}

/*!
 * \brief Returns the id of the node at \a index.
 * \param index An index of this model.
 */
QString CollectionTreeModel::idAt(const QModelIndex &index) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    return static_cast<const Item *>(index.internalPointer())->id;
}

/*!
 * \internal
 * \brief Returns \a ids without unknown ids and those that lie inside another of them.
 */
QStringList CollectionTreeModel::outermost(const QStringList &ids) const
{
    QStringList result;
    for (const QString &id : ids) {
        const Item *item = m_items.value(id);
        if (!item || result.contains(id))
            continue;
        bool nested = false;
        for (const Item *above = item->parent; above && !nested; above = above->parent)
            nested = ids.contains(above->id) && above != m_root.get();
        if (!nested)
            result.append(id);
    }
    return result;
}

/*!
 * \brief Returns the index of the node \a id.
 * \param id The node.
 */
QModelIndex CollectionTreeModel::indexOfNode(const QString &id) const
{
    const Item *item = m_items.value(id);
    return item ? indexOf(item) : QModelIndex();
}

/*!
 * \brief Returns the name of the node \a id.
 * \param id The node.
 * \return The name, or an empty string when the node does not exist.
 */
QString CollectionTreeModel::nodeName(const QString &id) const
{
    const Item *item = m_items.value(id);
    return item ? item->name : QString();
}

/*!
 * \brief Returns the request to open first.
 *
 * A workspace such as the bundled samples names it in its settings, so that
 * the guide and the application start at the same request. An unknown id or a
 * folder falls back to the first request in tree order.
 */
QString CollectionTreeModel::startRequestId() const
{
    const WorkspaceData &data = m_manager->workspaceData();
    const QString named = data.settings.value(QLatin1StringView("startRequest")).toString();
    const CollectionNode *node = named.isEmpty() ? nullptr : findNode(data.collection, named);
    if (node && node->kind == CollectionNode::Kind::Request)
        return named;

    const std::function<QString(const QList<CollectionNode> &)> first = [&first](const QList<CollectionNode> &nodes) {
        for (const CollectionNode &candidate : nodes) {
            if (candidate.kind == CollectionNode::Kind::Request)
                return candidate.id;
            if (const QString inside = first(candidate.children); !inside.isEmpty())
                return inside;
        }
        return QString();
    };
    return first(data.collection);
}

/*!
 * \brief Returns whether \a id is a folder.
 * \param id The node.
 */
bool CollectionTreeModel::isFolder(const QString &id) const
{
    const Item *item = m_items.value(id);
    return item && item->isFolder;
}

/*!
 * \brief Returns the folder a new item belongs to.
 *
 * With a folder selected the new item goes into it, with a request selected it
 * goes next to that request.
 * \param id The selected node, or an empty string.
 * \return The folder id, or an empty string for the top level.
 */
QString CollectionTreeModel::parentForNewItem(const QString &id) const
{
    const Item *item = m_items.value(id);
    if (!item)
        return {};
    if (item->isFolder)
        return item->id;
    return item->parent->id;
}

/*!
 * \brief Returns the request of the node \a id.
 * \param id The node.
 * \return The request, valid until the collection changes; null for folders and
 *         unknown ids.
 */
const RequestSpec *CollectionTreeModel::request(const QString &id) const
{
    const CollectionNode *node = findNode(m_manager->workspaceData().collection, id);
    return node && node->kind == CollectionNode::Kind::Request ? &node->request : nullptr;
}

/*!
 * \brief Replaces the request of the node \a id.
 * \param id The node.
 * \param request The new request.
 * \return False for a folder or an unknown node.
 */
bool CollectionTreeModel::updateRequest(const QString &id, const RequestSpec &request)
{
    QList<CollectionNode> collection = m_manager->workspaceData().collection;
    if (!setNodeRequest(collection, id, request))
        return false;
    commit(collection);

    Item *item = m_items.value(id);
    const QString method = QString::fromLatin1(httpMethodVerb(request.method));
    if (item->method != method) {
        item->method = method;
        const QModelIndex index = indexOf(item);
        emit dataChanged(index, index, {MethodRole});
    }
    return true;
}

/*!
 * \brief Returns the settings of the folder \a id.
 * \param id The node.
 * \return The settings, or null for a request or an unknown id.
 */
const FolderSettings *CollectionTreeModel::folderSettings(const QString &id) const
{
    const CollectionNode *node = findNode(m_manager->workspaceData().collection, id);
    return node && node->kind == CollectionNode::Kind::Folder ? &node->folder : nullptr;
}

/*!
 * \brief Replaces the settings of the folder \a id.
 *
 * The tree shows nothing of them, so no row changes.
 * \param id The folder.
 * \param settings The new settings.
 * \return False for a request or an unknown id.
 */
bool CollectionTreeModel::setFolderSettings(const QString &id, const FolderSettings &settings)
{
    QList<CollectionNode> collection = m_manager->workspaceData().collection;
    CollectionNode *node = findNode(collection, id);
    if (!node || node->kind != CollectionNode::Kind::Folder)
        return false;
    if (node->folder == settings)
        return true;
    node->folder = settings;
    commit(collection);
    return true;
}

/*!
 * \internal
 * \brief Stores \a collection in the workspace, which marks it dirty.
 */
void CollectionTreeModel::commit(const QList<CollectionNode> &collection)
{
    WorkspaceData data = m_manager->workspaceData();
    data.collection = collection;
    m_mirrored = collection;
    m_manager->updateWorkspaceData(data);
}

/*!
 * \internal
 * \brief Rebuilds the mirror unless it already matches the workspace.
 */
void CollectionTreeModel::syncWithWorkspace()
{
    const QList<CollectionNode> &collection = m_manager->workspaceData().collection;
    if (collection == m_mirrored)
        return;

    beginResetModel();
    m_items.clear();
    m_root->children.clear();
    m_mirrored = collection;
    for (const CollectionNode &node : collection)
        m_root->children.push_back(buildItem(node, m_root.get()));
    endResetModel();

    emit collectionChanged();
}

/*!
 * \internal
 * \brief Creates the mirror of \a node below \a parent and registers it by id.
 */
std::unique_ptr<CollectionTreeModel::Item> CollectionTreeModel::buildItem(const CollectionNode &node,
                                                                          Item *parent)
{
    auto item = std::make_unique<Item>();
    item->id = node.id;
    item->name = node.name;
    item->parent = parent;
    item->isFolder = node.kind == CollectionNode::Kind::Folder;
    if (!item->isFolder)
        item->method = QString::fromLatin1(httpMethodVerb(node.request.method));
    m_items.insert(item->id, item.get());

    for (const CollectionNode &child : node.children)
        item->children.push_back(buildItem(child, item.get()));
    return item;
}

/*!
 * \internal
 * \brief Removes \a item and everything below it from the id lookup.
 */
void CollectionTreeModel::forget(const Item *item)
{
    m_items.remove(item->id);
    for (const std::unique_ptr<Item> &child : item->children)
        forget(child.get());
}

/*!
 * \internal
 * \brief Returns the row of \a item below its parent.
 */
int CollectionTreeModel::rowOf(const Item *item) const
{
    const auto &siblings = item->parent->children;
    const auto found = std::find_if(siblings.cbegin(), siblings.cend(),
                                    [item](const std::unique_ptr<Item> &sibling) {
                                        return sibling.get() == item;
                                    });
    return int(found - siblings.cbegin());
}

/*!
 * \internal
 * \brief Returns the index of \a item; the invalid index for the root.
 */
QModelIndex CollectionTreeModel::indexOf(const Item *item) const
{
    if (item == m_root.get())
        return {};
    return createIndex(rowOf(item), 0, item);
}
