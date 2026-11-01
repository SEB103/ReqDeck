// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VARIABLERESOLVER_H
#define VARIABLERESOLVER_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "core/http/requestspec.h"

/**
 * Substitutes {{name}} references with variable values.
 *
 * Substitution is a single, case-sensitive pass: substituted values are not
 * scanned again, and references to unknown names stay verbatim in the output.
 */
class VariableResolver
{
public:
    /** Creates a resolver without variables. */
    VariableResolver() = default;

    /** Creates a resolver over the name-to-value map \a variables. */
    explicit VariableResolver(QHash<QString, QString> variables);

    /**
     * Creates a resolver from environment rows: disabled rows and rows with an
     * empty key are skipped, keys are trimmed, and a later row overrides an
     * earlier one with the same key.
     */
    static VariableResolver fromKeyValues(const QList<KeyValue> &variables);

    /**
     * Returns \a text with every known reference replaced. Unknown names are
     * appended to \a unresolved (when not null) unless already present.
     */
    QString resolve(const QString &text, QStringList *unresolved = nullptr) const;

    /** Returns the unique variable names referenced by \a text in order of appearance. */
    static QStringList referencedNames(const QString &text);

private:
    /** Variable values by name. */
    QHash<QString, QString> m_variables;
};

#endif // VARIABLERESOLVER_H
