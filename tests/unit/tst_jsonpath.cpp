// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "core/http/jsonpath.h"

/*!
 * \internal
 * \brief Returns the store example of RFC 9535 section 1.5.
 */
static QJsonValue store()
{
    return QJsonDocument::fromJson(R"({"store": {
        "book": [
          {"category": "reference", "author": "Nigel Rees", "title": "Sayings of the Century", "price": 8.95},
          {"category": "fiction", "author": "Evelyn Waugh", "title": "Sword of Honour", "price": 12.99},
          {"category": "fiction", "author": "Herman Melville", "title": "Moby Dick", "isbn": "0-553-21311-3", "price": 8.99},
          {"category": "fiction", "author": "J. R. R. Tolkien", "title": "The Lord of the Rings", "isbn": "0-395-19395-8", "price": 22.99}
        ],
        "bicycle": {"color": "red", "price": 399}}})").object();
}

/*! Verifies the JSONPath subset. */
class JsonPathTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies the RFC 9535 examples the subset supports. */
    void evaluatesRfcExamples_data();
    /*! Runs one example. */
    void evaluatesRfcExamples();

    /*! Verifies that paths outside the subset are refused with a message. */
    void refusesBadPaths_data();
    /*! Runs one bad path. */
    void refusesBadPaths();

    /*! Verifies that a query over a large body stays fast and bounded. */
    void filterIsLinearOnLargeBodies();

    /*! Verifies that an index too large fails and a huge slice step cannot overflow. */
    void numbersAndStepsAreSafe();
};

void JsonPathTest::evaluatesRfcExamples_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<int>("count");
    QTest::addColumn<QString>("first");

    QTest::newRow("authors") << QStringLiteral("$.store.book[*].author") << 4 << QStringLiteral("\"Nigel Rees\"");
    QTest::newRow("all authors") << QStringLiteral("$..author") << 4 << QStringLiteral("\"Nigel Rees\"");
    QTest::newRow("store children") << QStringLiteral("$.store.*") << 2 << QString();
    QTest::newRow("prices") << QStringLiteral("$.store..price") << 5 << QString();
    QTest::newRow("third book") << QStringLiteral("$..book[2].title") << 1 << QStringLiteral("\"Moby Dick\"");
    QTest::newRow("last book") << QStringLiteral("$..book[-1].title") << 1 << QStringLiteral("\"The Lord of the Rings\"");
    QTest::newRow("first two") << QStringLiteral("$..book[0,1].title") << 2 << QStringLiteral("\"Sayings of the Century\"");
    QTest::newRow("slice") << QStringLiteral("$..book[:2].title") << 2 << QStringLiteral("\"Sayings of the Century\"");
    QTest::newRow("step") << QStringLiteral("$..book[0:4:2].title") << 2 << QStringLiteral("\"Sayings of the Century\"");
    QTest::newRow("with isbn") << QStringLiteral("$..book[?(@.isbn)].title") << 2 << QStringLiteral("\"Moby Dick\"");
    QTest::newRow("cheap") << QStringLiteral("$..book[?(@.price < 10)].title") << 2 << QStringLiteral("\"Sayings of the Century\"");
    QTest::newRow("by name") << QStringLiteral("$..book[?@.author == 'Herman Melville'].price") << 1 << QStringLiteral("8.99");
    QTest::newRow("bracket name") << QStringLiteral("$['store']['bicycle'].color") << 1 << QStringLiteral("\"red\"");
    QTest::newRow("root") << QStringLiteral("$") << 1 << QString();
    QTest::newRow("missing") << QStringLiteral("$.store.car") << 0 << QString();
    QTest::newRow("out of range") << QStringLiteral("$..book[10]") << 0 << QString();
}

void JsonPathTest::evaluatesRfcExamples()
{
    QFETCH(QString, path);
    QFETCH(int, count);
    QFETCH(QString, first);

    const JsonPathResult result = JsonPath::evaluate(store(), path);
    QVERIFY2(result.ok, qPrintable(result.errorString));
    QCOMPARE(result.matches.size(), count);
    if (!first.isEmpty()) {
        const QJsonValue value = result.matches.at(0);
        const QString text = value.isString() ? u'"' + value.toString() + u'"' : QString::number(value.toDouble());
        QCOMPARE(text, first);
    }
}

void JsonPathTest::refusesBadPaths_data()
{
    QTest::addColumn<QString>("path");
    QTest::newRow("no root") << QStringLiteral("store.book");
    QTest::newRow("open bracket") << QStringLiteral("$.store[0");
    QTest::newRow("open quote") << QStringLiteral("$['store]");
    QTest::newRow("script") << QStringLiteral("$..book[(@.length-1)]");
    QTest::newRow("zero step") << QStringLiteral("$..book[::0]");
    QTest::newRow("function") << QStringLiteral("$..book[?(length(@.title) > 3)]");
}

void JsonPathTest::refusesBadPaths()
{
    QFETCH(QString, path);
    const JsonPathResult result = JsonPath::evaluate(store(), path);
    QVERIFY(!result.ok);
    QVERIFY(!result.errorString.isEmpty());
}

void JsonPathTest::filterIsLinearOnLargeBodies()
{
    QJsonArray items;
    for (int i = 0; i < 100000; ++i)
        items.append(QJsonObject {{QStringLiteral("id"), i}, {QStringLiteral("tag"), i % 2 ? QStringLiteral("odd") : QStringLiteral("even")}});
    const QJsonObject root {{QStringLiteral("items"), items}};

    QElapsedTimer timer;
    timer.start();
    const JsonPathResult result = JsonPath::evaluate(root, QStringLiteral("$.items[?(@.tag == 'odd')].id"));
    QVERIFY(result.ok);
    QCOMPARE(result.matches.size(), 50000);
    QVERIFY2(timer.elapsed() < 5000, qPrintable(QString::number(timer.elapsed())));

    QJsonArray wide;
    for (int i = 0; i < 400000; ++i)
        wide.append(QJsonArray {i, i, i});
    const JsonPathResult bounded = JsonPath::evaluate(QJsonObject {{QStringLiteral("w"), wide}}, QStringLiteral("$..*"));
    QVERIFY(!bounded.ok);
    QVERIFY(bounded.errorString.contains(QStringLiteral("more than")));
}

void JsonPathTest::numbersAndStepsAreSafe()
{
    const QJsonValue root = QJsonArray({0, 1, 2});
    QVERIFY(!JsonPath::evaluate(root, QStringLiteral("$[99999999999999999999]")).ok);
    const JsonPathResult huge = JsonPath::evaluate(root, QStringLiteral("$[1::9223372036854775807]"));
    QVERIFY(huge.ok);
    QCOMPARE(huge.matches, QJsonArray({1}));
    QCOMPARE(JsonPath::evaluate(root, QStringLiteral("$[0::2]")).matches, QJsonArray({0, 2}));
}

QTEST_GUILESS_MAIN(JsonPathTest)

#include "tst_jsonpath.moc"
