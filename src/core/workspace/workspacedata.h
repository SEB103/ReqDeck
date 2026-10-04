// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKSPACEDATA_H
#define WORKSPACEDATA_H

#include <QJsonArray>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QString>

/** Current .reqdeck JSON schema version written by this build. */
inline constexpr int kWorkspaceSchemaVersion = 1;

/** File-name suffix of workspace files, including the leading dot. */
inline constexpr QLatin1StringView kWorkspaceFileSuffix(".reqdeck");

/**
 * In-memory representation of one ReqDeck workspace file (.reqdeck).
 *
 * A workspace file is the single source of truth for workspace-specific state:
 * the request collection, the environments, the active environment, and
 * per-workspace settings. The collection, the environments, and the settings
 * are kept as JSON values and passed through unchanged; typed request and
 * environment models replace them once the HTTP domain exists.
 */
struct WorkspaceData
{
    /** Schema version of the file this data was read from or is written to. */
    int schemaVersion {kWorkspaceSchemaVersion};
    /** Human-readable workspace name shown in the launcher and window title. */
    QString displayName;
    /** Collection tree (folders and requests), passed through unchanged. */
    QJsonArray collection;
    /** Environments with their variables, passed through unchanged. */
    QJsonArray environments;
    /** Identifier of the active environment, or empty when none is active. */
    QString activeEnvironmentId;
    /** Per-workspace settings, passed through unchanged. */
    QJsonObject settings;

    /** Returns whether every field equals the corresponding field of \a other. */
    bool operator==(const WorkspaceData &other) const = default;
};

#endif // WORKSPACEDATA_H
