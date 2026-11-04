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
 * \brief Returns the rows of the active environment of \a data.
 *
 * The rows feed VariableResolver::fromKeyValues(), which applies the enabled
 * flag; disabled rows are therefore included here.
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
        for (const EnvironmentVariable &variable : environment.variables)
            rows.append({variable.key, variable.value, variable.enabled});
        break;
    }
    return rows;
}
