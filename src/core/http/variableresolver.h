// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VARIABLERESOLVER_H
#define VARIABLERESOLVER_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

#include "core/http/requestspec.h"

/** Place of one {{name}} reference in a text. */
struct VariableReference
{
    /** Position of the opening braces. */
    qsizetype start {0};
    /** Length of the reference, braces included. */
    qsizetype length {0};
    /** Referenced name; for a response() template its canonical text, for example response("Login", "$.token"). */
    QString name;
    /** Whether this is a response() template rather than a variable. */
    bool response {false};
};

/**
 * Substitutes {{name}} references with variable values.
 *
 * Substitution is a single, case-sensitive pass: substituted values are not
 * scanned again, and references to unknown names stay verbatim in the output.
 * A {{response("request", "selector")}} template reads a value through the
 * response lookup; without a value it behaves like an unknown name.
 */
class VariableResolver
{
public:
    /** Reads a value from the last response of a request, given its id or name and a selector. */
    using ResponseLookup = std::function<std::optional<QString>(const QString &request, const QString &selector)>;

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

    /** Returns whether a reference to \a name resolves. */
    bool contains(const QString &name) const;

    /** Sets the lookup that response() templates read from; none leaves them unresolved. */
    void setResponseLookup(ResponseLookup lookup);

    /** Returns the canonical template text that reads \a selector from the response of \a request. */
    static QString responseTemplate(const QString &request, const QString &selector);

    /** Returns the unique variable names referenced by \a text in order of appearance. */
    static QStringList referencedNames(const QString &text);

    /** Returns every reference in \a text in order of appearance. */
    static QList<VariableReference> references(const QString &text);

private:
    /** Variable values by name. */
    QHash<QString, QString> m_variables;

    /** Lookup of response() templates; empty for none. */
    ResponseLookup m_responseLookup;
};

#endif // VARIABLERESOLVER_H
