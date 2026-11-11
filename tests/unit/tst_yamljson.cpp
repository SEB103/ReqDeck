// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

#include "core/import/yamljson.h"

/*!
 * \internal
 * \brief Returns the content of the test data file \a name.
 */
static QByteArray testData(const char *name)
{
    QFile file(QFINDTESTDATA(QStringLiteral("data/import/") + QLatin1StringView(name)));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/*! Verifies the conversion of YAML documents to JSON. */
class YamlJsonTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies how plain and quoted scalars are typed. */
    void typesScalars_data();
    /*! Verifies how plain and quoted scalars are typed. */
    void typesScalars();

    /*! Verifies that the YAML form of a document equals its JSON form. */
    void matchesJsonDocument();

    /*! Verifies that anchors and aliases are expanded. */
    void expandsAliases();

    /*! Verifies that a document expanding to too many values is refused. */
    void refusesAliasBomb();

    /*! Verifies that "<<" merge keys merge, with the mapping's own keys winning. */
    void mergesKeys();

    /*! Verifies that a mapping used as a key is refused. */
    void refusesComplexKeys();

    /*! Verifies that too deep nesting is refused. */
    void refusesDeepNesting();

    /*! Verifies that broken YAML and a lone scalar give an error. */
    void reportsErrors();
};

void YamlJsonTest::typesScalars_data()
{
    QTest::addColumn<QByteArray>("yaml");
    QTest::addColumn<QJsonValue>("expected");

    QTest::newRow("string") << QByteArray("v: hello") << QJsonValue(QStringLiteral("hello"));
    QTest::newRow("integer") << QByteArray("v: 42") << QJsonValue(42);
    QTest::newRow("negative") << QByteArray("v: -7") << QJsonValue(-7);
    QTest::newRow("hex") << QByteArray("v: 0x1F") << QJsonValue(31);
    QTest::newRow("octal") << QByteArray("v: 0o17") << QJsonValue(15);
    QTest::newRow("float") << QByteArray("v: 1.5e3") << QJsonValue(1500.0);
    QTest::newRow("true") << QByteArray("v: True") << QJsonValue(true);
    QTest::newRow("false") << QByteArray("v: false") << QJsonValue(false);
    QTest::newRow("null") << QByteArray("v: ~") << QJsonValue(QJsonValue::Null);
    QTest::newRow("empty") << QByteArray("v:") << QJsonValue(QJsonValue::Null);
    QTest::newRow("quoted number") << QByteArray("v: \"42\"") << QJsonValue(QStringLiteral("42"));
    QTest::newRow("single quoted") << QByteArray("v: 'true'") << QJsonValue(QStringLiteral("true"));
    QTest::newRow("tagged string") << QByteArray("v: !!str 10") << QJsonValue(QStringLiteral("10"));
    QTest::newRow("yes is text") << QByteArray("v: yes") << QJsonValue(QStringLiteral("yes"));
    QTest::newRow("version") << QByteArray("v: 3.0.3") << QJsonValue(QStringLiteral("3.0.3"));
    QTest::newRow("infinity") << QByteArray("v: .inf") << QJsonValue(QStringLiteral(".inf"));
}

void YamlJsonTest::typesScalars()
{
    QFETCH(QByteArray, yaml);
    QFETCH(QJsonValue, expected);

    QString error;
    const std::optional<QJsonDocument> json = YamlJson::toJson(yaml, &error);
    QVERIFY2(json, qPrintable(error));
    const QJsonValue value = json->object().value(QLatin1StringView("v"));
    QCOMPARE(value.type(), expected.type());
    QCOMPARE(value, expected);
}

void YamlJsonTest::matchesJsonDocument()
{
    QString error;
    const std::optional<QJsonDocument> fromYaml = YamlJson::toJson(testData("openapi-petshop.yaml"), &error);
    QVERIFY2(fromYaml, qPrintable(error));
    QCOMPARE(*fromYaml, QJsonDocument::fromJson(testData("openapi-petshop.json")));
}

void YamlJsonTest::expandsAliases()
{
    const std::optional<QJsonDocument> json = YamlJson::toJson("base: &b {x: 1}\nuse: *b\nlist: [*b, *b]");
    QVERIFY(json);
    QCOMPARE(json->object().value(QLatin1StringView("use")).toObject().value(QLatin1StringView("x")).toInt(), 1);
    QCOMPARE(json->object().value(QLatin1StringView("list")).toArray().size(), 2);
}

void YamlJsonTest::refusesAliasBomb()
{
    // Each level holds ten references to the one before: 10^9 values in a few lines.
    QByteArray yaml = "a0: &a0 [x, x, x, x, x, x, x, x, x, x]\n";
    for (int level = 1; level < 9; ++level) {
        QByteArray refs;
        for (int i = 0; i < 10; ++i)
            refs += (i ? ", *a" : "*a") + QByteArray::number(level - 1);
        yaml += "a" + QByteArray::number(level) + ": &a" + QByteArray::number(level) + " [" + refs + "]\n";
    }

    QElapsedTimer timer;
    timer.start();
    QString error;
    QVERIFY(!YamlJson::toJson(yaml, &error));
    QVERIFY2(error.contains(QStringLiteral("more than")), qPrintable(error));
    QVERIFY2(timer.elapsed() < 2000, qPrintable(QString::number(timer.elapsed())));
}

void YamlJsonTest::mergesKeys()
{
    const std::optional<QJsonDocument> json =
        YamlJson::toJson("base: &b {x: 1, y: 2}\nuse:\n  <<: *b\n  y: 3\n");
    QVERIFY(json);
    const QJsonObject use = json->object().value(QLatin1StringView("use")).toObject();
    QCOMPARE(use.value(QLatin1StringView("x")).toInt(), 1);
    QCOMPARE(use.value(QLatin1StringView("y")).toInt(), 3);
    QVERIFY(!use.contains(QLatin1StringView("<<")));
}

void YamlJsonTest::refusesComplexKeys()
{
    QString error;
    QVERIFY(!YamlJson::toJson("? {a: 1}\n: value\n", &error));
    QVERIFY2(error.contains(QStringLiteral("key")), qPrintable(error));
}

void YamlJsonTest::refusesDeepNesting()
{
    const QByteArray yaml = QByteArray(YamlJson::kMaxDepth + 10, '[') + QByteArray(YamlJson::kMaxDepth + 10, ']');
    QString error;
    QVERIFY(!YamlJson::toJson(yaml, &error));
    QVERIFY2(!error.isEmpty(), "no error message");
}

void YamlJsonTest::reportsErrors()
{
    QString error;
    QVERIFY(!YamlJson::toJson("a: [1, 2\nb: 3", &error));
    QVERIFY2(error.contains(QStringLiteral("line")), qPrintable(error));
    QVERIFY(!YamlJson::toJson("just text", &error));
    QVERIFY2(error.contains(QStringLiteral("no mapping")), qPrintable(error));
}

QTEST_GUILESS_MAIN(YamlJsonTest)

#include "tst_yamljson.moc"
