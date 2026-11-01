// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "core/http/variableresolver.h"

/*!
 * \internal
 * \brief Returns a resolver with baseUrl and id defined.
 */
static VariableResolver makeResolver()
{
    return VariableResolver(QHash<QString, QString> {
        {QStringLiteral("baseUrl"), QStringLiteral("http://h")},
        {QStringLiteral("id"), QStringLiteral("7")},
    });
}

/*! Verifies {{variable}} substitution and reference scanning. */
class VariableResolverTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that known references are replaced by their values. */
    void substitutesKnownNames();

    /*! Verifies that whitespace inside the braces is allowed. */
    void allowsInnerWhitespace();

    /*! Verifies that unknown references stay verbatim and are reported once. */
    void keepsUnresolvedAndReportsOnce();

    /*! Verifies that reporting appends to an existing list without duplicates. */
    void appendsWithoutDuplicates();

    /*! Verifies that substituted values are not expanded again. */
    void doesNotRecurse();

    /*! Verifies that malformed references are treated as literal text. */
    void ignoresMalformed();

    /*! Verifies that variable names are case-sensitive. */
    void isCaseSensitive();

    /*! Verifies filtering and override order of fromKeyValues(). */
    void fromKeyValuesFiltersAndOverrides();

    /*! Verifies that referencedNames() returns unique names in order. */
    void referencedNamesAreUnique();
};

void VariableResolverTest::substitutesKnownNames()
{
    QStringList unresolved;
    QCOMPARE(makeResolver().resolve(QStringLiteral("{{baseUrl}}/users/{{id}}"), &unresolved),
             QStringLiteral("http://h/users/7"));
    QVERIFY(unresolved.isEmpty());
}

void VariableResolverTest::allowsInnerWhitespace()
{
    QCOMPARE(makeResolver().resolve(QStringLiteral("{{ id }}")), QStringLiteral("7"));
}

void VariableResolverTest::keepsUnresolvedAndReportsOnce()
{
    QStringList unresolved;
    const QString text = QStringLiteral("{{a}}{{b}}{{a}}");
    QCOMPARE(VariableResolver().resolve(text, &unresolved), text);
    QCOMPARE(unresolved, QStringList({QStringLiteral("a"), QStringLiteral("b")}));
}

void VariableResolverTest::appendsWithoutDuplicates()
{
    QStringList unresolved {QStringLiteral("a")};
    VariableResolver().resolve(QStringLiteral("{{a}}{{c}}"), &unresolved);
    QCOMPARE(unresolved, QStringList({QStringLiteral("a"), QStringLiteral("c")}));
}

void VariableResolverTest::doesNotRecurse()
{
    const VariableResolver resolver(QHash<QString, QString> {
        {QStringLiteral("x"), QStringLiteral("{{y}}")},
        {QStringLiteral("y"), QStringLiteral("1")},
    });
    QStringList unresolved;
    QCOMPARE(resolver.resolve(QStringLiteral("{{x}}"), &unresolved), QStringLiteral("{{y}}"));
    QVERIFY(unresolved.isEmpty());
}

void VariableResolverTest::ignoresMalformed()
{
    QStringList unresolved;
    const QString text = QStringLiteral("{{}} {{a b}} {x}");
    QCOMPARE(makeResolver().resolve(text, &unresolved), text);
    QVERIFY(unresolved.isEmpty());
}

void VariableResolverTest::isCaseSensitive()
{
    QStringList unresolved;
    QCOMPARE(makeResolver().resolve(QStringLiteral("{{ID}}"), &unresolved), QStringLiteral("{{ID}}"));
    QCOMPARE(unresolved, QStringList({QStringLiteral("ID")}));
}

void VariableResolverTest::fromKeyValuesFiltersAndOverrides()
{
    const VariableResolver resolver = VariableResolver::fromKeyValues({
        {QStringLiteral("a"), QStringLiteral("1"), false},
        {QStringLiteral(" b "), QStringLiteral("2"), true},
        {QStringLiteral("b"), QStringLiteral("3"), true},
        {QString(), QStringLiteral("4"), true},
    });
    QStringList unresolved;
    QCOMPARE(resolver.resolve(QStringLiteral("{{a}}|{{b}}"), &unresolved), QStringLiteral("{{a}}|3"));
    QCOMPARE(unresolved, QStringList({QStringLiteral("a")}));
}

void VariableResolverTest::referencedNamesAreUnique()
{
    QCOMPARE(VariableResolver::referencedNames(QStringLiteral("{{a}}{{ b }}{{a}}")),
             QStringList({QStringLiteral("a"), QStringLiteral("b")}));
}

QTEST_GUILESS_MAIN(VariableResolverTest)

#include "tst_variableresolver.moc"
