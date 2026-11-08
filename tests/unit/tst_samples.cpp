// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSet>
#include <QtTest>

#include "core/http/requestbuilder.h"
#include "core/http/variableresolver.h"
#include "core/workspace/collectiontree.h"
#include "core/workspace/workspaceserializer.h"

/*!
 * \internal
 * \brief Appends every request below \a nodes to \a requests.
 */
static void collectRequests(const QList<CollectionNode> &nodes, QList<RequestSpec> &requests)
{
    for (const CollectionNode &node : nodes) {
        if (node.kind == CollectionNode::Kind::Request)
            requests.append(node.request);
        else
            collectRequests(node.children, requests);
    }
}

/*! Verifies the bundled samples workspace without sending anything. */
class SamplesTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Loads the samples file shared by all test functions. */
    void initTestCase();

    /*! Verifies the identity of the workspace and its active environment. */
    void hasActiveEnvironmentWithBaseUrl();

    /*! Verifies that all seven HTTP methods occur. */
    void coversEveryMethod();

    /*! Verifies that every body mode and the auth schemes occur. */
    void coversBodiesAndAuth();

    /*! Verifies that every request builds with the active environment alone. */
    void everyRequestBuildsWithoutUnresolvedVariables();

    /*! Verifies that requests only address the public test services. */
    void usesOnlyPublicTestHosts();

    /*! Verifies that the file has no secrets, which would be lost when seeded. */
    void hasNoSecretVariables();

    /*! Verifies that the request opened first exists and needs no other request. */
    void startRequestExists();

private:
    /*! The loaded workspace. */
    WorkspaceData m_data;

    /*! All requests of the workspace. */
    QList<RequestSpec> m_requests;
};

void SamplesTest::initTestCase()
{
    const WorkspaceSerializer::LoadResult result =
        WorkspaceSerializer::load(QStringLiteral(REQDECK_SAMPLES_FILE));
    QVERIFY2(result.ok, qPrintable(result.errorString));
    m_data = result.data;
    collectRequests(m_data.collection, m_requests);
    QVERIFY(!m_requests.isEmpty());
}

void SamplesTest::hasActiveEnvironmentWithBaseUrl()
{
    QCOMPARE(m_data.displayName, QStringLiteral("Samples"));
    QVERIFY(!m_data.id.isEmpty());

    const VariableResolver variables =
        VariableResolver::fromKeyValues(activeEnvironmentVariables(m_data));
    QCOMPARE(variables.resolve(QStringLiteral("{{baseUrl}}")), QStringLiteral("https://httpbin.org"));
}

void SamplesTest::coversEveryMethod()
{
    QSet<HttpMethod> methods;
    for (const RequestSpec &request : std::as_const(m_requests))
        methods.insert(request.method);
    QCOMPARE(methods.size(), 7);
}

void SamplesTest::coversBodiesAndAuth()
{
    QSet<RawBodyType> rawTypes;
    QSet<AuthType> authTypes;
    bool form = false;
    for (const RequestSpec &request : std::as_const(m_requests)) {
        if (request.body.mode == BodyMode::Raw)
            rawTypes.insert(request.body.rawType);
        form = form || request.body.mode == BodyMode::FormUrlEncoded;
        authTypes.insert(request.auth.type);
    }
    QVERIFY(rawTypes.contains(RawBodyType::Json));
    QVERIFY(rawTypes.contains(RawBodyType::Xml));
    QVERIFY(form);
    QVERIFY(authTypes.contains(AuthType::Basic));
    QVERIFY(authTypes.contains(AuthType::Bearer));
}

void SamplesTest::everyRequestBuildsWithoutUnresolvedVariables()
{
    const VariableResolver variables =
        VariableResolver::fromKeyValues(activeEnvironmentVariables(m_data));
    for (const RequestSpec &request : std::as_const(m_requests)) {
        const BuiltRequest built = RequestBuilder::build(request, variables, {});
        QVERIFY2(built.ok, qPrintable(request.url + u": " + built.errorString));
        QVERIFY2(built.unresolvedVariables.isEmpty(),
                 qPrintable(built.unresolvedVariables.join(u',')));
    }
}

void SamplesTest::usesOnlyPublicTestHosts()
{
    const QSet<QString> allowed {QStringLiteral("httpbin.org"),
                                 QStringLiteral("jsonplaceholder.typicode.com")};
    const VariableResolver variables =
        VariableResolver::fromKeyValues(activeEnvironmentVariables(m_data));
    for (const RequestSpec &request : std::as_const(m_requests)) {
        const BuiltRequest built = RequestBuilder::build(request, variables, {});
        QVERIFY2(allowed.contains(built.request.url().host()), qPrintable(request.url));
        QCOMPARE(built.request.url().scheme(), QStringLiteral("https"));
    }
}

void SamplesTest::hasNoSecretVariables()
{
    for (const Environment &environment : std::as_const(m_data.environments)) {
        for (const EnvironmentVariable &variable : environment.variables)
            QVERIFY(!variable.secret);
    }
}

QTEST_GUILESS_MAIN(SamplesTest)

void SamplesTest::startRequestExists()
{
    const QString id = m_data.settings.value(QLatin1StringView("startRequest")).toString();
    QCOMPARE(id, QStringLiteral("placeholder-post"));
    const CollectionNode *node = findNode(m_data.collection, id);
    QVERIFY(node);
    QCOMPARE(node->kind, CollectionNode::Kind::Request);
}

#include "tst_samples.moc"
