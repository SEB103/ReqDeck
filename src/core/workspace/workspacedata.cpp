// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/workspacedata.h"

#include <QUuid>

/*!
 * \brief Returns a new UUID without braces.
 */
QString newItemId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

/*!
 * \brief Returns whether \a id is a UUID in canonical form.
 *
 * The id names a file in the secrets directory and comes from a file that may
 * have been shared. QUuid::fromString() alone is not enough: it parses a prefix
 * and ignores what follows, so "<uuid>/../x" would pass. Comparing against the
 * canonical text rejects every other character.
 * \param id The text to check.
 */
bool isValidWorkspaceId(const QString &id)
{
    const QUuid uuid = QUuid::fromString(id);
    return !uuid.isNull()
           && id.compare(uuid.toString(QUuid::WithoutBraces), Qt::CaseInsensitive) == 0;
}

/*!
 * \brief Returns the rows of the active environment of \a data.
 *
 * The rows feed VariableResolver::fromKeyValues(), which applies the enabled
 * flag; disabled rows are therefore included here. A secret without a value,
 * as on a machine it was not entered on, is left out: the resolver then reports
 * the reference instead of treating it as an empty value.
 * \param data The workspace.
 * \return The rows, or an empty list when no environment is active.
 */
QList<KeyValue> activeEnvironmentVariables(const WorkspaceData &data)
{
    QList<KeyValue> rows;
    if (data.activeEnvironmentId.isEmpty())
        return rows;
    for (const Environment &environment : data.environments) {
        if (environment.id != data.activeEnvironmentId)
            continue;
        for (const EnvironmentVariable &variable : environment.variables) {
            if (!variable.secret || !variable.value.isEmpty())
                rows.append({variable.key, variable.value, variable.enabled});
        }
        break;
    }
    return rows;
}
