// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/variableresolver.h"

#include <QRegularExpression>

#include <utility>

namespace {
/*!
 * \internal
 * \brief Returns the pattern of one {{name}} reference or response() template.
 *
 * Whitespace inside the braces is allowed. A name, captured as 3, must start
 * with a letter or underscore and may contain letters, digits, '_', '.', and
 * '-'. A template {{response("request", "selector")}} captures its two quoted
 * arguments as 1 and 2, in which a backslash escapes a quote or a backslash.
 */
const QRegularExpression &referencePattern()
{
    static const QRegularExpression pattern(QStringLiteral(
        R"re(\{\{\s*(?:response\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\)|([A-Za-z_][A-Za-z0-9_.\-]*))\s*\}\})re"));
    return pattern;
}

/*!
 * \internal
 * \brief Returns the quoted argument \a text with its escapes undone.
 */
QString unquoted(QStringView text)
{
    QString result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == u'\\' && i + 1 < text.size())
            ++i;
        result += text.at(i);
    }
    return result;
}

/*!
 * \internal
 * \brief Returns \a text quoted for a response() template.
 */
QString quoted(const QString &text)
{
    QString result = text;
    result.replace(u'\\', QLatin1StringView("\\\\"));
    result.replace(u'"', QLatin1StringView("\\\""));
    return u'"' + result + u'"';
}

/*!
 * \internal
 * \brief Returns the reference \a match describes.
 */
VariableReference referenceOf(const QRegularExpressionMatch &match)
{
    VariableReference reference;
    reference.start = match.capturedStart();
    reference.length = match.capturedLength();
    reference.response = match.capturedLength(3) == 0;
    reference.name = reference.response
                         ? VariableResolver::responseTemplate(unquoted(match.capturedView(1)),
                                                              unquoted(match.capturedView(2)))
                         : match.captured(3);
    return reference;
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

        const VariableReference reference = referenceOf(match);
        const QString &name = reference.name;
        if (reference.response && m_responseLookup) {
            if (const std::optional<QString> value =
                    m_responseLookup(unquoted(match.capturedView(1)), unquoted(match.capturedView(2)))) {
                result += *value;
                continue;
            }
        }
        // A template without a response may still have a value typed for this send.
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
 * \brief Sets the lookup that response() templates read from.
 *
 * The lookup is called during resolve() with the unquoted request and
 * selector of each template; a value it returns is copied verbatim and not
 * scanned again. An empty lookup leaves templates to typed values only.
 * \param lookup The lookup, or an empty function.
 */
void VariableResolver::setResponseLookup(ResponseLookup lookup)
{
    m_responseLookup = std::move(lookup);
}

/*!
 * \brief Returns the canonical text of the template reading \a selector from \a request.
 *
 * References report a template by this text, so the same template is named the
 * same way whatever whitespace it was typed with.
 * \param request Id or name of the request.
 * \param selector A JSONPath, or "header:Name".
 */
QString VariableResolver::responseTemplate(const QString &request, const QString &selector)
{
    return QLatin1StringView("response(") + quoted(request) + QLatin1StringView(", ") + quoted(selector) + u')';
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
        const QString name = referenceOf(it.next()).name;
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
    while (it.hasNext())
        references.append(referenceOf(it.next()));
    return references;
}
