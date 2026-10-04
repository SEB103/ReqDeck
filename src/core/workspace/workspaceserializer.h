// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKSPACESERIALIZER_H
#define WORKSPACESERIALIZER_H

#include <QString>

#include "core/workspace/workspacedata.h"

QT_BEGIN_NAMESPACE
class QJsonObject;
QT_END_NAMESPACE

/**
 * Reads and writes WorkspaceData to and from .reqdeck JSON files.
 *
 * All functions are static; the class has no state. Loading validates the file
 * and reports a typed error instead of throwing, so a missing or malformed
 * workspace file never crashes the caller.
 */
class WorkspaceSerializer
{
public:
    /** Reason a load() or save() call failed. */
    enum class Error {
        /** No error; the operation succeeded. */
        None,
        /** The workspace file does not exist. */
        FileNotFound,
        /** The workspace file could not be opened for reading. */
        ReadFailed,
        /** The file content is not a valid JSON object. */
        InvalidJson,
        /** The JSON is well-formed but does not describe a workspace. */
        InvalidSchema,
        /** The file uses a schema version this build cannot read. */
        UnsupportedVersion,
        /** The workspace file could not be written. */
        WriteFailed
    };

    /** Outcome of a load() call: parsed data plus success and error details. */
    struct LoadResult
    {
        /** Whether the load succeeded. */
        bool ok {false};
        /** Failure reason, or Error::None on success. */
        Error error {Error::None};
        /** Human-readable error text, empty on success. */
        QString errorString;
        /** Parsed workspace data; only meaningful when ok is true. */
        WorkspaceData data;
    };

    /** Loads and validates the workspace file at \a filePath. */
    static LoadResult load(const QString &filePath);

    /**
     * Writes \a data as pretty-printed JSON to \a filePath, creating parent
     * directories as needed. Returns true on success; on failure returns false
     * and sets \a errorString when it is not null.
     */
    static bool save(const QString &filePath, const WorkspaceData &data,
                     QString *errorString = nullptr);

    /** Serializes \a data into a JSON object. */
    static QJsonObject toJson(const WorkspaceData &data);

    /** Returns a human-readable description of \a error. */
    static QString errorToString(Error error);
};

#endif // WORKSPACESERIALIZER_H
