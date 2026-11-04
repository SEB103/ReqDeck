// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SECRETSTORE_H
#define SECRETSTORE_H

#include <QString>

#include "core/workspace/workspacedata.h"

/**
 * Keeps the values of secret environment variables outside workspace files.
 *
 * Each workspace with secret values has one JSON file named after its UUID in
 * the store directory. The values are not encrypted: the store keeps them out
 * of files people share, it does not protect them on this machine.
 */
class SecretStore
{
public:
    /** Creates a store over \a directory, which is created on the first write. */
    explicit SecretStore(QString directory);

    /** Returns the path of the secrets file of \a workspaceId. */
    QString filePath(const QString &workspaceId) const;

    /**
     * Fills the secret variables of \a data that have a stored value. A missing
     * file is not an error. A malformed file is renamed to "<file>.corrupt";
     * then \a data stays unchanged and false is returned with \a errorString set.
     */
    bool applyTo(WorkspaceData &data, QString *errorString = nullptr) const;

    /**
     * Replaces the stored values of \a data with its non-empty secret values and
     * removes the file when none is left. Fails for an id that is not a UUID.
     */
    bool save(const WorkspaceData &data, QString *errorString = nullptr) const;

private:
    /** Directory holding one file per workspace. */
    QString m_directory;
};

#endif // SECRETSTORE_H
