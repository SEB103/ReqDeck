// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SECRETSTORE_H
#define SECRETSTORE_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>

#include "core/workspace/secretcipher.h"
#include "core/workspace/workspacedata.h"

/**
 * Keeps the values of secret environment variables outside workspace files.
 *
 * Each workspace with secret values has one JSON file named after its UUID in
 * the store directory. The values, and their keys, are encrypted for the
 * current Windows user; files of schema 1 hold them in plain text and are still
 * read. A key maps to an array, because an environment may hold several rows
 * with the same key, for example to switch between two values.
 */
class SecretStore
{
public:
    /** Outcome of reading the secrets file of a workspace. */
    enum class ReadStatus {
        /** The file was read, or the workspace has none. */
        Ok,
        /** The file was malformed and renamed to "<file>.corrupt"; nothing is left to protect. */
        Corrupt,
        /** The file could not be read or moved and is still there; do not overwrite it. */
        Unreadable,
        /** The file is encrypted for another user or computer and stays as it is. */
        Unavailable
    };

    /** Creates a store over \a directory, which is created on the first write, encrypting with \a cipher. */
    explicit SecretStore(QString directory, SecretCipher cipher = SecretCipher());

    /** Returns the path of the secrets file of \a workspaceId. */
    QString filePath(const QString &workspaceId) const;

    /** Fills the secret variables of \a data that have a stored value. */
    ReadStatus applyTo(WorkspaceData &data) const;

    /** Like applyTo(), but keeps values already present in \a data. */
    ReadStatus fillEmptyValues(WorkspaceData &data) const;

    /**
     * Replaces the stored values of \a data with its secret values and removes
     * the file when no value is left. Fails for an invalid workspace id.
     */
    bool save(const WorkspaceData &data, QString *errorString = nullptr) const;

    /** Returns whether the secrets file of \a workspaceId holds plain values of schema 1. */
    bool isPlain(const QString &workspaceId) const;

    /** Returns the secret values of \a data as save() stores them, to tell whether they changed. */
    static QJsonObject valuesOf(const WorkspaceData &data);

    /** Returns the content of the secrets file; nullopt when missing or unreadable. */
    std::optional<QByteArray> snapshot(const QString &workspaceId) const;

    /** Puts back a snapshot(); nullopt removes the file. */
    bool restore(const QString &workspaceId, const std::optional<QByteArray> &content) const;

private:
    /** Reads the file into \a data; with \a onlyEmpty existing values are kept. */
    ReadStatus read(WorkspaceData &data, bool onlyEmpty) const;

    /** Directory holding one file per workspace. */
    QString m_directory;

    /** Encrypts and decrypts the values. */
    SecretCipher m_cipher;
};

#endif // SECRETSTORE_H
