// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "core/http/responsecache.h"
#include "core/http/responsedata.h"
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

    /*! Verifies the position, length, and name of each reference. */
    void referencesReportPositions();

    /*! Verifies that a malformed name is not a reference. */
    void referencesSkipInvalidNames();

    /*! Verifies that contains() sees what fromKeyValues() kept. */
    void containsHonorsEnabledAndKey();

    /*! Verifies that a response() template reads through the lookup, or stays unresolved by its canonical name. */
    void responseTemplateUsesLookup();

    /*! Verifies quotes and backslashes in template arguments and the canonical text. */
    void responseTemplateEscapes();

    /*! Verifies that a template without a response takes a value typed for its canonical name. */
    void responseTemplateTakesTypedValue();

    /*! Verifies the selectors of the response cache: JSONPath, header, and what gives nothing. */
    void responseCacheSelectors();

    /*! Verifies that the cache picks the newest of several requests and forgets the ones not retained. */
    void responseCacheFindsRequests();
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

void VariableResolverTest::referencesReportPositions()
{
    const QList<VariableReference> references =
        VariableResolver::references(QStringLiteral("a{{ x }}b{{y}}"));

    QCOMPARE(references.size(), 2);
    QCOMPARE(references.at(0).start, qsizetype(1));
    QCOMPARE(references.at(0).length, qsizetype(7));
    QCOMPARE(references.at(0).name, QStringLiteral("x"));
    QCOMPARE(references.at(1).start, qsizetype(9));
    QCOMPARE(references.at(1).length, qsizetype(5));
    QCOMPARE(references.at(1).name, QStringLiteral("y"));
}

void VariableResolverTest::referencesSkipInvalidNames()
{
    QVERIFY(VariableResolver::references(QStringLiteral("{{1a}}")).isEmpty());
}

void VariableResolverTest::containsHonorsEnabledAndKey()
{
    const VariableResolver resolver = VariableResolver::fromKeyValues({
        {QStringLiteral(" a "), QStringLiteral("1"), true},
        {QStringLiteral("b"), QStringLiteral("2"), false},
    });

    QVERIFY(resolver.contains(QStringLiteral("a")));
    QVERIFY(!resolver.contains(QStringLiteral("b")));
}

void VariableResolverTest::responseTemplateUsesLookup()
{
    VariableResolver resolver = makeResolver();
    const QString text = QStringLiteral(R"(Bearer {{ response( "Login" , "$.token" ) }} at {{baseUrl}})");

    QStringList unresolved;
    QCOMPARE(resolver.resolve(text, &unresolved), QStringLiteral(R"(Bearer {{ response( "Login" , "$.token" ) }} at http://h)"));
    QCOMPARE(unresolved, QStringList({QStringLiteral(R"(response("Login", "$.token"))")}));

    QStringList seen;
    resolver.setResponseLookup([&seen](const QString &request, const QString &selector) -> std::optional<QString> {
        seen = {request, selector};
        return QStringLiteral("t-1");
    });
    unresolved.clear();
    QCOMPARE(resolver.resolve(text, &unresolved), QStringLiteral("Bearer t-1 at http://h"));
    QVERIFY(unresolved.isEmpty());
    QCOMPARE(seen, QStringList({QStringLiteral("Login"), QStringLiteral("$.token")}));

    const QList<VariableReference> references = VariableResolver::references(text);
    QCOMPARE(references.size(), 2);
    QVERIFY(references.at(0).response);
    QCOMPARE(references.at(0).start, 7);
    QCOMPARE(references.at(0).name, QStringLiteral(R"(response("Login", "$.token"))"));
    QVERIFY(!references.at(1).response);

    // A variable called response is still a variable.
    QCOMPARE(VariableResolver(QHash<QString, QString> {{QStringLiteral("response"), QStringLiteral("r")}})
                 .resolve(QStringLiteral("{{response}}")),
             QStringLiteral("r"));
}

void VariableResolverTest::responseTemplateEscapes()
{
    VariableResolver resolver;
    QString request;
    QString selector;
    resolver.setResponseLookup([&](const QString &r, const QString &s) -> std::optional<QString> {
        request = r;
        selector = s;
        return std::nullopt;
    });
    QStringList unresolved;
    resolver.resolve(QStringLiteral(R"({{response("Say \"hi\"", "$['a\\b']")}})"), &unresolved);
    QCOMPARE(request, QStringLiteral(R"(Say "hi")"));
    QCOMPARE(selector, QStringLiteral(R"($['a\b'])"));
    const QString canonical = QStringLiteral(R"(response("Say \"hi\"", "$['a\\b']"))");
    QCOMPARE(unresolved, QStringList({canonical}));
    QCOMPARE(VariableResolver::responseTemplate(request, selector), unresolved.first());
    QVERIFY(VariableResolver::references(QStringLiteral(R"({{response("a")}})")).isEmpty());
}

void VariableResolverTest::responseTemplateTakesTypedValue()
{
    const QString name = VariableResolver::responseTemplate(QStringLiteral("Login"), QStringLiteral("$.token"));
    VariableResolver resolver = VariableResolver::fromKeyValues({{name, QStringLiteral("typed"), true}});
    resolver.setResponseLookup([](const QString &, const QString &) { return std::optional<QString>(); });
    QCOMPARE(resolver.resolve(QStringLiteral(R"({{response("Login","$.token")}})")), QStringLiteral("typed"));
}

void VariableResolverTest::responseCacheSelectors()
{
    ResponseData response;
    response.statusCode = 200;
    response.headers = {{QByteArrayLiteral("x-token"), QByteArrayLiteral("h-1")}};
    response.body = R"({"token":"t-1","n":2.5,"ok":true,"list":[1,2],"obj":{"a":null}})";
    ResponseCache cache;
    cache.store(QStringLiteral("login"), response);

    const auto value = [&cache](const QString &selector) {
        return cache.value({QStringLiteral("login")}, selector);
    };
    QCOMPARE(value(QStringLiteral("$.token")), std::optional<QString>(QStringLiteral("t-1")));
    QCOMPARE(value(QStringLiteral("$.n")), std::optional<QString>(QStringLiteral("2.5")));
    QCOMPARE(value(QStringLiteral("$.ok")), std::optional<QString>(QStringLiteral("true")));
    QCOMPARE(value(QStringLiteral("$.list")), std::optional<QString>(QStringLiteral("[1,2]")));
    QCOMPARE(value(QStringLiteral("$.obj")), std::optional<QString>(QStringLiteral(R"({"a":null})")));
    QCOMPARE(value(QStringLiteral("$.list[*]")), std::optional<QString>(QStringLiteral("1")));
    QCOMPARE(value(QStringLiteral("header:X-Token")), std::optional<QString>(QStringLiteral("h-1")));
    QVERIFY(!value(QStringLiteral("header:Missing")));
    QVERIFY(!value(QStringLiteral("$.missing")));
    QVERIFY(!value(QStringLiteral("token")));

    // A body that is not JSON keeps only the headers.
    ResponseData html = response;
    html.body = "<html></html>";
    cache.store(QStringLiteral("page"), html);
    QVERIFY(!cache.value({QStringLiteral("page")}, QStringLiteral("$")));
    QCOMPARE(cache.value({QStringLiteral("page")}, QStringLiteral("header:x-token")),
             std::optional<QString>(QStringLiteral("h-1")));

    // A failed send keeps the earlier response.
    ResponseData failed;
    failed.error = ResponseError::Timeout;
    cache.store(QStringLiteral("login"), failed);
    QCOMPARE(value(QStringLiteral("$.token")), std::optional<QString>(QStringLiteral("t-1")));
}

void VariableResolverTest::responseCacheFindsRequests()
{
    ResponseData first;
    first.statusCode = 200;
    first.body = R"({"v":"old"})";
    ResponseData second = first;
    second.body = R"({"v":"new"})";
    ResponseCache cache;
    QVERIFY(cache.isEmpty());
    cache.store(QStringLiteral("a"), first);
    cache.store(QStringLiteral("b"), second);

    QVERIFY(cache.contains(QStringLiteral("a")));
    QCOMPARE(cache.value({QStringLiteral("a")}, QStringLiteral("$.v")), std::optional<QString>(QStringLiteral("old")));
    // Of several requests, the one answered last counts.
    QCOMPARE(cache.value({QStringLiteral("a"), QStringLiteral("b")}, QStringLiteral("$.v")),
             std::optional<QString>(QStringLiteral("new")));
    QVERIFY(!cache.value({QStringLiteral("other")}, QStringLiteral("$.v")));
    QVERIFY(!cache.value({}, QStringLiteral("$.v")));
    QCOMPARE(cache.requestIds(), QStringList({QStringLiteral("b"), QStringLiteral("a")}));

    cache.retain({QStringLiteral("a")});
    QCOMPARE(cache.requestIds(), QStringList({QStringLiteral("a")}));

    cache.clear();
    QVERIFY(cache.isEmpty());
    QVERIFY(!cache.value({QStringLiteral("a")}, QStringLiteral("$.v")));
}

QTEST_GUILESS_MAIN(VariableResolverTest)

#include "tst_variableresolver.moc"
