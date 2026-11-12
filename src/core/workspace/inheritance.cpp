// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/inheritance.h"

namespace {
/*!
 * \internal
 * \brief Collects the folders from the top of \a nodes down to the node \a id into \a path.
 * \return Whether the node was found; \a path is complete only then.
 */
bool findPath(const QList<CollectionNode> &nodes, const QString &id, QList<const CollectionNode *> &path)
{
    for (const CollectionNode &node : nodes) {
        if (node.id == id)
            return true;
        if (node.kind != CollectionNode::Kind::Folder)
            continue;
        path.append(&node);
        if (findPath(node.children, id, path))
            return true;
        path.removeLast();
    }
    return false;
}
} // namespace

/*!
 * \brief Returns the auth and variables the request \a requestId inherits.
 *
 * Postman works the same way: a request or folder set to inherit takes the auth
 * of its parent, up to the first that sets one. A folder with no auth stops the
 * search, so a subtree can opt out. Variables of all folders above apply, an
 * inner folder over an outer one; environment variables come after them and win.
 * \param collection The collection of the workspace.
 * \param requestId The request.
 * \param own The auth of the request itself.
 * \return The effective auth and the folder variables, outermost first.
 */
InheritedSettings inheritedSettings(const QList<CollectionNode> &collection, const QString &requestId,
                                    const RequestAuth &own)
{
    InheritedSettings settings;
    settings.auth = own.type == AuthType::Inherit ? RequestAuth() : own;
    settings.authOwnerId = requestId;

    QList<const CollectionNode *> path;
    if (!findPath(collection, requestId, path))
        return settings;

    for (const CollectionNode *folder : std::as_const(path))
        settings.variables += folder->folder.variables;

    if (own.type == AuthType::Inherit) {
        for (auto it = path.crbegin(); it != path.crend(); ++it) {
            const RequestAuth &auth = (*it)->folder.auth;
            if (auth.type != AuthType::Inherit) {
                settings.auth = auth;
                settings.authOwnerId = (*it)->id;
                break;
            }
        }
    }
    return settings;
}
