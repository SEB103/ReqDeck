// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IMPORTRESULT_H
#define IMPORTRESULT_H

#include <QList>
#include <QString>
#include <QStringList>

#include "core/workspace/workspacedata.h"

/** What an importer read from another tool's file. */
struct ImportResult
{
    /** Whether the file could be read at all; when false only errorString is meaningful. */
    bool ok {false};
    /** Why the file could not be read. */
    QString errorString;
    /** Name of the format, for example "Postman collection". */
    QString format;
    /** Whether folder holds a collection; false for a file with environments only. */
    bool hasCollection {false};
    /** One top-level folder with everything the file held, ids already assigned. */
    CollectionNode folder;
    /** Environments the file held, ids already assigned. */
    QList<Environment> environments;
    /** What did not carry over, one line per item, as "<where>: <what>". */
    QStringList report;
};

#endif // IMPORTRESULT_H
