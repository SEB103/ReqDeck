// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef POSTMANIMPORTER_H
#define POSTMANIMPORTER_H

#include <QByteArray>

#include "core/import/importresult.h"

QT_BEGIN_NAMESPACE
class QJsonObject;
QT_END_NAMESPACE

/**
 * Reads Postman collections (format 2.0 and 2.1) and Postman environments.
 *
 * Nothing in the file runs: scripts are reported and dropped, as are features
 * ReqDeck does not have. All functions are static.
 */
class PostmanImporter
{
public:
    /** Returns whether \a root is a Postman collection. */
    static bool isCollection(const QJsonObject &root);

    /** Returns whether \a root is a Postman environment. */
    static bool isEnvironment(const QJsonObject &root);

    /** Reads the collection in \a json into one folder, with a report of what was dropped. */
    static ImportResult importCollection(const QByteArray &json);

    /** Reads the environment in \a json. */
    static ImportResult importEnvironment(const QByteArray &json);
};

#endif // POSTMANIMPORTER_H
