// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "core/workspace/requestjson.h"

/*!
 * \internal
 * \brief Returns a request with every field set to a non-default value.
 */
static RequestSpec makeFullRequest(HttpMethod method)
{
    RequestSpec spec;
    spec.method = method;
    spec.url = QStringLiteral("{{baseUrl}}/items");
    spec.queryParams = {{QStringLiteral("a"), QStringLiteral("1"), true},
                        {QStringLiteral("b"), QStringLiteral("2"), false}};
    spec.headers = {{QStringLiteral("Accept"), QStringLiteral("*/*"), true},
                    {QStringLiteral("X-Off"), QStringLiteral("x"), false}};
    spec.body.mode = BodyMode::Raw;
    spec.body.rawType = RawBodyType::Xml;
    spec.body.raw = QStringLiteral("<a/>");
    spec.body.form = {{QStringLiteral("f"), QStringLiteral("v"), false}};
    spec.auth.type = AuthType::ApiKey;
    spec.auth.username = QStringLiteral("user");
    spec.auth.password = QStringLiteral("pass");
    spec.auth.token = QStringLiteral("tok");
    spec.auth.apiKeyName = QStringLiteral("key");
    spec.auth.apiKeyValue = QStringLiteral("{{secret}}");
    spec.auth.apiKeyLocation = ApiKeyLocation::Query;
    spec.timeoutMs = 5000;
    return spec;
}

/*!
 * \internal
 * \brief Parses the JSON object text \a json.
 */
static QJsonObject parseObject(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object();
}

/*! Verifies the JSON form of RequestSpec. */
class RequestJsonTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that every field survives a round trip for every method. */
    void roundTripPreservesEveryField_data();
    /*! Runs one round-trip case. */
    void roundTripPreservesEveryField();

    /*! Verifies a round trip of form and text bodies with each auth type. */
    void roundTripPreservesBodyAndAuthModes_data();
    /*! Runs one body and auth mode case. */
    void roundTripPreservesBodyAndAuthModes();

    /*! Verifies that non-ASCII text and line breaks are preserved. */
    void roundTripKeepsNonAscii();

    /*! Verifies that default values are not written. */
    void writesOnlyNonDefaultMembers();

    /*! Verifies that fields of an inactive auth type are preserved. */
    void keepsInactiveFields();

    /*! Verifies that absent members take their default values. */
    void missingMembersUseDefaults();

    /*! Verifies the errors for members of the wrong type or value. */
    void rejectsInvalidMembers_data();
    /*! Runs one invalid-member case. */
    void rejectsInvalidMembers();

    /*! Verifies that unknown members are ignored. */
    void ignoresUnknownMembers();
};

void RequestJsonTest::roundTripPreservesEveryField_data()
{
    QTest::addColumn<HttpMethod>("method");

    QTest::newRow("GET") << HttpMethod::Get;
    QTest::newRow("POST") << HttpMethod::Post;
    QTest::newRow("PUT") << HttpMethod::Put;
    QTest::newRow("PATCH") << HttpMethod::Patch;
    QTest::newRow("DELETE") << HttpMethod::Delete;
    QTest::newRow("HEAD") << HttpMethod::Head;
    QTest::newRow("OPTIONS") << HttpMethod::Options;
}

void RequestJsonTest::roundTripPreservesEveryField()
{
    QFETCH(HttpMethod, method);

    const RequestSpec spec = makeFullRequest(method);
    QString error;
    const std::optional<RequestSpec> parsed = requestFromJson(requestToJson(spec), &error);
    QVERIFY2(parsed, qPrintable(error));
    QVERIFY(*parsed == spec);
}

void RequestJsonTest::roundTripPreservesBodyAndAuthModes_data()
{
    QTest::addColumn<BodyMode>("bodyMode");
    QTest::addColumn<RawBodyType>("rawType");
    QTest::addColumn<AuthType>("authType");

    QTest::newRow("form-basic") << BodyMode::FormUrlEncoded << RawBodyType::Json << AuthType::Basic;
    QTest::newRow("text-bearer") << BodyMode::Raw << RawBodyType::Text << AuthType::Bearer;
    QTest::newRow("json-none") << BodyMode::Raw << RawBodyType::Json << AuthType::None;
    QTest::newRow("xml-inherit") << BodyMode::Raw << RawBodyType::Xml << AuthType::Inherit;
}

void RequestJsonTest::roundTripPreservesBodyAndAuthModes()
{
    QFETCH(BodyMode, bodyMode);
    QFETCH(RawBodyType, rawType);
    QFETCH(AuthType, authType);

    RequestSpec spec = makeFullRequest(HttpMethod::Post);
    spec.body.mode = bodyMode;
    spec.body.rawType = rawType;
    spec.auth.type = authType;
    spec.auth.apiKeyLocation = ApiKeyLocation::Header;

    const std::optional<RequestSpec> parsed = requestFromJson(requestToJson(spec));
    QVERIFY(parsed);
    QVERIFY(*parsed == spec);
}

void RequestJsonTest::roundTripKeepsNonAscii()
{
    RequestSpec spec;
    spec.url = QStringLiteral("https://h/Müller");
    spec.headers = {{QStringLiteral("X-Price"), QStringLiteral("10 €"), true}};
    spec.body.mode = BodyMode::Raw;
    spec.body.raw = QStringLiteral("line1\nline2 \U0001F600");

    const QByteArray text = QJsonDocument(requestToJson(spec)).toJson();
    const std::optional<RequestSpec> parsed = requestFromJson(QJsonDocument::fromJson(text).object());
    QVERIFY(parsed);
    QVERIFY(*parsed == spec);
}

void RequestJsonTest::writesOnlyNonDefaultMembers()
{
    QStringList keys = requestToJson(RequestSpec {}).keys();
    keys.sort();
    QCOMPARE(keys, QStringList({QStringLiteral("method"), QStringLiteral("url")}));

    RequestSpec spec;
    spec.headers = {{QStringLiteral("A"), QStringLiteral("1"), true}};
    const QJsonObject row = requestToJson(spec).value(QStringLiteral("headers")).toArray().at(0).toObject();
    QVERIFY(!row.contains(QStringLiteral("enabled")));

    spec.headers[0].enabled = false;
    const QJsonObject disabled =
        requestToJson(spec).value(QStringLiteral("headers")).toArray().at(0).toObject();
    QCOMPARE(disabled.value(QStringLiteral("enabled")).toBool(true), false);
}

void RequestJsonTest::keepsInactiveFields()
{
    RequestSpec spec;
    spec.auth.type = AuthType::None;
    spec.auth.token = QStringLiteral("kept");

    const std::optional<RequestSpec> parsed = requestFromJson(requestToJson(spec));
    QVERIFY(parsed);
    QCOMPARE(parsed->auth.token, QStringLiteral("kept"));
}

void RequestJsonTest::missingMembersUseDefaults()
{
    const std::optional<RequestSpec> parsed = requestFromJson(parseObject("{\"method\":\"GET\",\"url\":\"x\"}"));
    QVERIFY(parsed);
    RequestSpec expected;
    expected.url = QStringLiteral("x");
    QVERIFY(*parsed == expected);
}

void RequestJsonTest::rejectsInvalidMembers_data()
{
    QTest::addColumn<QByteArray>("json");

    QTest::newRow("method") << QByteArray("{\"method\":\"FETCH\",\"url\":\"x\"}");
    QTest::newRow("method-number") << QByteArray("{\"method\":1,\"url\":\"x\"}");
    QTest::newRow("url-number") << QByteArray("{\"url\":5}");
    QTest::newRow("params-object") << QByteArray("{\"queryParams\":{}}");
    QTest::newRow("row-not-object") << QByteArray("{\"headers\":[1]}");
    QTest::newRow("row-key-number") << QByteArray("{\"headers\":[{\"key\":1}]}");
    QTest::newRow("row-enabled-string") << QByteArray("{\"headers\":[{\"key\":\"a\",\"enabled\":\"no\"}]}");
    QTest::newRow("body-string") << QByteArray("{\"body\":\"x\"}");
    QTest::newRow("body-mode") << QByteArray("{\"body\":{\"mode\":\"multipart\"}}");
    QTest::newRow("raw-type") << QByteArray("{\"body\":{\"rawType\":\"yaml\"}}");
    QTest::newRow("auth-type") << QByteArray("{\"auth\":{\"type\":\"digest\"}}");
    QTest::newRow("key-location") << QByteArray("{\"auth\":{\"apiKeyLocation\":\"cookie\"}}");
    QTest::newRow("timeout-negative") << QByteArray("{\"timeoutMs\":-1}");
    QTest::newRow("timeout-fraction") << QByteArray("{\"timeoutMs\":1.5}");
    QTest::newRow("timeout-string") << QByteArray("{\"timeoutMs\":\"5\"}");
}

void RequestJsonTest::rejectsInvalidMembers()
{
    QFETCH(QByteArray, json);

    QString error;
    QVERIFY(!requestFromJson(parseObject(json), &error));
    QVERIFY(!error.isEmpty());
}

void RequestJsonTest::ignoresUnknownMembers()
{
    QVERIFY(requestFromJson(parseObject("{\"url\":\"x\",\"future\":{\"a\":1}}")));
}

QTEST_GUILESS_MAIN(RequestJsonTest)

#include "tst_requestjson.moc"
