// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OPENAPIIMPORTER_H
#define OPENAPIIMPORTER_H

#include <QByteArray>

#include "core/import/importresult.h"

QT_BEGIN_NAMESPACE
class QJsonObject;
QT_END_NAMESPACE

/**
 * Reads OpenAPI 3 documents (JSON) into a folder of requests.
 *
 * Operations are grouped into folders by their first tag. The first server
 * becomes the folder variable baseUrl, and each server an environment when
 * there are several. Security schemes become folder or request auth with
 * {{token}}-style variables to fill in.
 */
class OpenApiImporter
{
public:
    /** Returns whether \a root looks like an OpenAPI 3 or Swagger 2.0 document. */
    static bool isDocument(const QJsonObject &root);

    /** Reads the document in \a json, with a report of what was dropped; Swagger 2.0 fails with a message. */
    static ImportResult importDocument(const QByteArray &json);
};

#endif // OPENAPIIMPORTER_H
