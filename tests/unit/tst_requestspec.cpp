// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "core/http/requestspec.h"
#include "core/http/responsedata.h"

/*! Verifies the HTTP request and response value types. */
class RequestSpecTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that every method survives a verb-to-method round trip. */
    void verbRoundTrip_data();
    /*! Runs one verb round-trip case. */
    void verbRoundTrip();

    /*! Verifies that method parsing ignores case and surrounding whitespace. */
    void parseIsCaseInsensitiveAndTrimmed();

    /*! Verifies that unknown or empty method names are rejected. */
    void parseRejectsUnknown();

    /*! Verifies the default state of RequestSpec and ResponseData. */
    void defaultsAreEmpty();

    /*! Verifies that equality takes nested fields into account. */
    void equalityComparesAllFields();
};

void RequestSpecTest::verbRoundTrip_data()
{
    QTest::addColumn<HttpMethod>("method");
    QTest::addColumn<QByteArray>("verb");

    QTest::newRow("get") << HttpMethod::Get << QByteArray("GET");
    QTest::newRow("post") << HttpMethod::Post << QByteArray("POST");
    QTest::newRow("put") << HttpMethod::Put << QByteArray("PUT");
    QTest::newRow("patch") << HttpMethod::Patch << QByteArray("PATCH");
    QTest::newRow("delete") << HttpMethod::Delete << QByteArray("DELETE");
    QTest::newRow("head") << HttpMethod::Head << QByteArray("HEAD");
    QTest::newRow("options") << HttpMethod::Options << QByteArray("OPTIONS");
}

void RequestSpecTest::verbRoundTrip()
{
    QFETCH(HttpMethod, method);
    QFETCH(QByteArray, verb);

    QCOMPARE(httpMethodVerb(method), verb);
    const std::optional<HttpMethod> parsed = httpMethodFromString(QString::fromLatin1(verb));
    QVERIFY(parsed.has_value());
    QCOMPARE(*parsed, method);
}

void RequestSpecTest::parseIsCaseInsensitiveAndTrimmed()
{
    QCOMPARE(httpMethodFromString(u" patch "), std::optional<HttpMethod>(HttpMethod::Patch));
    QCOMPARE(httpMethodFromString(u"delete"), std::optional<HttpMethod>(HttpMethod::Delete));
}

void RequestSpecTest::parseRejectsUnknown()
{
    QVERIFY(!httpMethodFromString(u"FETCH").has_value());
    QVERIFY(!httpMethodFromString(u"").has_value());
}

void RequestSpecTest::defaultsAreEmpty()
{
    const RequestSpec spec;
    QCOMPARE(spec.method, HttpMethod::Get);
    QVERIFY(spec.url.isEmpty());
    QCOMPARE(spec.body.mode, BodyMode::None);
    QCOMPARE(spec.auth.type, AuthType::None);
    QCOMPARE(spec.timeoutMs, 0);

    const ResponseData response;
    QVERIFY(!response.hasStatus());
    QCOMPARE(response.error, ResponseError::None);
}

void RequestSpecTest::equalityComparesAllFields()
{
    RequestSpec lhs;
    lhs.url = QStringLiteral("https://example.test/");
    lhs.headers.append({QStringLiteral("Accept"), QStringLiteral("*/*"), true});
    RequestSpec rhs = lhs;
    QVERIFY(lhs == rhs);

    rhs.headers[0].enabled = false;
    QVERIFY(!(lhs == rhs));
}

QTEST_GUILESS_MAIN(RequestSpecTest)

#include "tst_requestspec.moc"
