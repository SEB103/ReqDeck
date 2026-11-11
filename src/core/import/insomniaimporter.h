// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INSOMNIAIMPORTER_H
#define INSOMNIAIMPORTER_H

#include <QByteArray>

#include "core/import/importresult.h"

QT_BEGIN_NAMESPACE
class QJsonObject;
QT_END_NAMESPACE

/**
 * Reads Insomnia exports of format 4 (JSON).
 *
 * The first workspace becomes one folder; its base environment becomes the
 * folder's variables and the other environments ReqDeck environments.
 * {{ _.name }} references become {{name}}. Nothing in the file runs.
 */
class InsomniaImporter
{
public:
    /** Returns whether \a root is an Insomnia export of format 4. */
    static bool isExport(const QJsonObject &root);

    /** Reads the export in \a json, with a report of what was dropped. */
    static ImportResult importExport(const QByteArray &json);
};

#endif // INSOMNIAIMPORTER_H
