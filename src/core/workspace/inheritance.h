// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INHERITANCE_H
#define INHERITANCE_H

#include <QList>
#include <QString>

#include "core/http/requestspec.h"
#include "core/workspace/workspacedata.h"

/** What a request takes from the folders that hold it. */
struct InheritedSettings
{
    /** Effective auth: the request's own, or that of the nearest folder above that sets one. */
    RequestAuth auth;
    /** Variable rows of the folders, outermost first, so that inner rows win. */
    QList<KeyValue> variables;
};

/**
 * Returns the settings the request \a requestId in \a collection inherits, with \a own as
 * its own auth. Inherit looks up the folders from the inside out; a folder with None stops
 * the search, and nothing found means no auth. An unknown request inherits nothing.
 */
InheritedSettings inheritedSettings(const QList<CollectionNode> &collection, const QString &requestId,
                                    const RequestAuth &own);

#endif // INHERITANCE_H
