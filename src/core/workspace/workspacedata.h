// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKSPACEDATA_H
#define WORKSPACEDATA_H

#include <QJsonObject>
#include <QLatin1StringView>
#include <QList>
#include <QString>

#include "core/http/requestspec.h"

/** Current .reqdeck JSON schema version written by this build. */
inline constexpr int kWorkspaceSchemaVersion = 2;

/** File-name suffix of workspace files, including the leading dot. */
inline constexpr QLatin1StringView kWorkspaceFileSuffix(".reqdeck");

/** Deepest nesting of collection items a workspace may have. */
inline constexpr int kMaxCollectionDepth = 64;

/** One variable of an environment. */
struct EnvironmentVariable
{
    /** Variable name, referenced as {{key}}. */
    QString key;
    /** Variable value; a secret value is kept out of the workspace file. */
    QString value;
    /** Whether the variable takes part in substitution. */
    bool enabled {true};
    /** Whether the value is secret and stored by SecretStore. */
    bool secret {false};

    bool operator==(const EnvironmentVariable &) const = default;
};

/** Named set of variables, for example "Staging". */
struct Environment
{
    /** Stable identifier, unique among the environments of a workspace. */
    QString id;
    /** Name shown in the environment selector. */
    QString name;
    /** Variables in display order. */
    QList<EnvironmentVariable> variables;

    bool operator==(const Environment &) const = default;
};

/** Folder or request in the collection tree. */
/** Settings of a folder that the requests and folders inside it inherit. */
struct FolderSettings
{
    /** Auth for requests inside that inherit it; Inherit passes the question further up. */
    RequestAuth auth {AuthType::Inherit};
    /** Variables for requests inside; environment variables win over them. */
    QList<KeyValue> variables;

    bool operator==(const FolderSettings &) const = default;
};

struct CollectionNode
{
    /** Kind of tree node. */
    enum class Kind {
        Folder,
        Request
    };

    /** Whether this node is a folder or a request. */
    Kind kind {Kind::Folder};
    /** Stable identifier, unique in the whole collection. */
    QString id;
    /** Name shown in the tree. */
    QString name;
    /** The request; used by Kind::Request only. */
    RequestSpec request;
    /** Settings inherited by what the folder holds; used by Kind::Folder only. */
    FolderSettings folder;
    /** Child nodes in display order; used by Kind::Folder only. */
    QList<CollectionNode> children;

    bool operator==(const CollectionNode &) const = default;
};

/**
 * In-memory representation of one ReqDeck workspace file (.reqdeck).
 *
 * A workspace file is the single source of truth for workspace-specific state:
 * the request collection, the environments, the active environment, and
 * per-workspace settings. The only exception are the values of secret
 * variables, which WorkspaceManager keeps in a SecretStore. The settings are
 * kept as JSON and passed through unchanged.
 */
struct WorkspaceData
{
    /** Schema version of the file this data was read from or is written to. */
    int schemaVersion {kWorkspaceSchemaVersion};
    /** UUID of the workspace, naming its secrets file; empty in older files. */
    QString id;
    /** Human-readable workspace name shown in the launcher and window title. */
    QString displayName;
    /** Collection tree (folders and requests). */
    QList<CollectionNode> collection;
    /** Environments with their variables. */
    QList<Environment> environments;
    /** Identifier of the active environment, or empty when none is active. */
    QString activeEnvironmentId;
    /** Per-workspace settings, passed through unchanged. */
    QJsonObject settings;

    /** Returns whether every field equals the corresponding field of \a other. */
    bool operator==(const WorkspaceData &other) const = default;
};

/** Returns a new UUID without braces for a workspace, node, or environment. */
QString newItemId();

/** Returns whether \a id is a UUID in canonical form, without braces or anything else. */
bool isValidWorkspaceId(const QString &id);

/**
 * Returns the rows of the active environment of \a data; empty when none is
 * active. A secret without a value is left out, so references to it are reported
 * as unresolved instead of silently sending an empty value.
 */
QList<KeyValue> activeEnvironmentVariables(const WorkspaceData &data);

#endif // WORKSPACEDATA_H
