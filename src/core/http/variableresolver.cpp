// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/variableresolver.h"

#include <QRegularExpression>

#include <utility>

namespace {
/*!
 * \internal
 * \brief Returns the pattern of one {{name}} reference.
 *
 * Whitespace inside the braces is allowed; the captured name must start with a
 * letter or underscore and may contain letters, digits, '_', '.', and '-'.
 */
const QRegularExpression &referencePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\{\{\s*([A-Za-z_][A-Za-z0-9_.\-]*)\s*\}\})"));
    return pattern;
}
} // namespace

/*!
 * \brief Creates a resolver over \a variables.
 * \param variables Variable values by name.
 */
VariableResolver::VariableResolver(QHash<QString, QString> variables)
    : m_variables(std::move(variables))
{
}

/*!
 * \brief Creates a resolver from environment rows.
 *
 * Disabled rows and rows whose trimmed key is empty are skipped. Rows are
 * applied in order, so a later row with the same key wins.
 * \param variables The environment rows.
 */
VariableResolver VariableResolver::fromKeyValues(const QList<KeyValue> &variables)
{
    QHash<QString, QString> values;
    for (const KeyValue &row : variables) {
        const QString key = row.key.trimmed();
        if (!row.enabled || key.isEmpty())
            continue;
        values.insert(key, row.value);
    }
    return VariableResolver(std::move(values));
}

/*!
 * \brief Replaces every known {{name}} reference in \a text.
 *
 * The scan runs once from left to right; substituted values are copied
 * verbatim and never re-scanned, so a value that itself contains a reference
 * cannot recurse. References to unknown names stay in the output unchanged.
 * \param text The text to resolve.
 * \param unresolved Optional list that receives each unknown name once; names
 *        already present are not appended again.
 * \return The resolved text.
 */
QString VariableResolver::resolve(const QString &text, QStringList *unresolved) const
{
    QString result;
    result.reserve(text.size());
    qsizetype position = 0;

    QRegularExpressionMatchIterator it = referencePattern().globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result += QStringView(text).sliced(position, match.capturedStart() - position);
        position = match.capturedEnd();

        const QString name = match.captured(1);
        const auto value = m_variables.constFind(name);
        if (value != m_variables.cend()) {
            result += *value;
            continue;
        }
        result += match.capturedView();
        if (unresolved && !unresolved->contains(name))
            unresolved->append(name);
    }
    result += QStringView(text).sliced(position);
    return result;
}

/*!
 * \brief Returns whether a reference to \a name resolves.
 * \param name The variable name.
 */
bool VariableResolver::contains(const QString &name) const
{
    return m_variables.contains(name);
}

/*!
 * \brief Returns the unique names referenced by \a text in order of appearance.
 * \param text The text to scan.
 */
QStringList VariableResolver::referencedNames(const QString &text)
{
    QStringList names;
    QRegularExpressionMatchIterator it = referencePattern().globalMatch(text);
    while (it.hasNext()) {
        const QString name = it.next().captured(1);
        if (!names.contains(name))
            names.append(name);
    }
    return names;
}

/*!
 * \brief Returns every reference in \a text in order of appearance.
 * \param text The text to scan.
 */
QList<VariableReference> VariableResolver::references(const QString &text)
{
    QList<VariableReference> references;
    QRegularExpressionMatchIterator it = referencePattern().globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        references.append({match.capturedStart(), match.capturedLength(), match.captured(1)});
    }
    return references;
}
