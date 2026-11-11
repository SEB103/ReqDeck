// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "core/workspace/inheritance.h"

/*!
 * \internal
 * \brief Returns a folder \a id holding \a children with \a auth.
 */
static CollectionNode folder(const QString &id, const RequestAuth &auth, const QList<CollectionNode> &children,
                             const QList<KeyValue> &variables = {})
{
    CollectionNode node;
    node.id = id;
    node.folder.auth = auth;
    node.folder.variables = variables;
    node.children = children;
    return node;
}

/*!
 * \internal
 * \brief Returns a request node \a id.
 */
static CollectionNode request(const QString &id)
{
    CollectionNode node;
    node.kind = CollectionNode::Kind::Request;
    node.id = id;
    return node;
}

/*!
 * \internal
 * \brief Returns auth of \a type with the token \a token.
 */
static RequestAuth auth(AuthType type, const QString &token = {})
{
    RequestAuth result;
    result.type = type;
    result.token = token;
    return result;
}

/*! Verifies which auth and variables a request takes from its folders. */
class InheritanceTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that a request with its own auth keeps it. */
    void requestAuthWinsOverFolder();

    /*! Verifies that Inherit finds the nearest folder that sets an auth. */
    void inheritsNearestFolderAuth();

    /*! Verifies that a folder without auth stops the search. */
    void inheritanceStopsAtNone();

    /*! Verifies that folder variables come outermost first, so inner ones win. */
    void variablesInnerWins();

    /*! Verifies that an unknown request inherits nothing and Inherit becomes no auth. */
    void unknownRequestInheritsNothing();
};

void InheritanceTest::requestAuthWinsOverFolder()
{
    const QList<CollectionNode> tree {folder(QStringLiteral("f"), auth(AuthType::Bearer, QStringLiteral("folder")),
                                             {request(QStringLiteral("r"))})};

    const InheritedSettings settings =
        inheritedSettings(tree, QStringLiteral("r"), auth(AuthType::Bearer, QStringLiteral("own")));

    QCOMPARE(settings.auth.token, QStringLiteral("own"));
}

void InheritanceTest::inheritsNearestFolderAuth()
{
    const QList<CollectionNode> tree {
        folder(QStringLiteral("outer"), auth(AuthType::Bearer, QStringLiteral("outer")),
               {folder(QStringLiteral("inner"), auth(AuthType::Inherit), {request(QStringLiteral("r"))})})};

    const InheritedSettings settings = inheritedSettings(tree, QStringLiteral("r"), auth(AuthType::Inherit));

    QCOMPARE(settings.auth.type, AuthType::Bearer);
    QCOMPARE(settings.auth.token, QStringLiteral("outer"));
}

void InheritanceTest::inheritanceStopsAtNone()
{
    const QList<CollectionNode> tree {
        folder(QStringLiteral("outer"), auth(AuthType::Bearer, QStringLiteral("outer")),
               {folder(QStringLiteral("inner"), auth(AuthType::None), {request(QStringLiteral("r"))})})};

    const InheritedSettings settings = inheritedSettings(tree, QStringLiteral("r"), auth(AuthType::Inherit));

    QCOMPARE(settings.auth.type, AuthType::None);
    QVERIFY(settings.auth.token.isEmpty());
}

void InheritanceTest::variablesInnerWins()
{
    const QList<CollectionNode> tree {folder(
        QStringLiteral("outer"), auth(AuthType::Inherit),
        {folder(QStringLiteral("inner"), auth(AuthType::Inherit), {request(QStringLiteral("r"))},
                {{QStringLiteral("base"), QStringLiteral("inner"), true}})},
        {{QStringLiteral("base"), QStringLiteral("outer"), true}, {QStringLiteral("page"), QStringLiteral("1"), true}})};

    const InheritedSettings settings = inheritedSettings(tree, QStringLiteral("r"), auth(AuthType::None));

    QCOMPARE(settings.variables.size(), 3);
    QCOMPARE(settings.variables.at(0).value, QStringLiteral("outer"));
    QCOMPARE(settings.variables.at(2).value, QStringLiteral("inner"));
    QCOMPARE(settings.auth.type, AuthType::None);
}

void InheritanceTest::unknownRequestInheritsNothing()
{
    const QList<CollectionNode> tree {folder(QStringLiteral("f"), auth(AuthType::Bearer, QStringLiteral("x")),
                                             {request(QStringLiteral("r"))}, {{QStringLiteral("a"), QStringLiteral("b"), true}})};

    const InheritedSettings settings = inheritedSettings(tree, QStringLiteral("missing"), auth(AuthType::Inherit));

    QCOMPARE(settings.auth.type, AuthType::None);
    QVERIFY(settings.variables.isEmpty());
}

QTEST_GUILESS_MAIN(InheritanceTest)

#include "tst_inheritance.moc"
