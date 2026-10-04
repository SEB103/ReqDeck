// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QtTest>

#include "framework/diagnosticslevel.h"
#include "framework/logfiltermodel.h"
#include "framework/logmodel.h"

/*!
 * \internal
 * \brief Appends \a count entries at \a level to \a model.
 */
static void appendEntries(LogModel &model, int level, int count,
                          const QString &prefix = QStringLiteral("message"))
{
    for (int i = 0; i < count; ++i)
        model.appendEntry(level, QString(), QStringLiteral("%1 %2").arg(prefix).arg(i));
}

/*! Verifies LogModel storage and LogFilterModel filtering. */
class LogModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that an appended entry is exposed through every role. */
    void appendExposesEntryThroughRoles();

    /*! Verifies that the log drops the oldest entries instead of growing forever. */
    void reachingTheCapDropsOldestEntries();

    /*! Verifies that clear() empties the log and reports the change. */
    void clearEmptiesTheLog();

    /*! Verifies that the default logging category is not shown as a category. */
    void defaultCategoryIsNotShown();

    /*! Verifies that the severity filter hides entries below the chosen level. */
    void severityFilterHidesLowerLevels();

    /*! Verifies that the text filter matches the message and the category. */
    void textFilterMatchesMessageAndCategory();

    /*! Verifies that the copy helper renders only the entries currently shown. */
    void visibleTextRendersShownEntriesOnly();
};

/*!
 * \brief Verifies that an appended entry is exposed through every role.
 */
void LogModelTest::appendExposesEntryThroughRoles()
{
    LogModel model;
    QSignalSpy countSpy(&model, &LogModel::countChanged);

    model.appendEntry(Diagnostics::Warning, QStringLiteral("http"),
                      QStringLiteral("endpoint refused"));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.count(), 1);
    QCOMPARE(countSpy.count(), 1);

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, LogModel::MessageRole).toString(),
             QStringLiteral("endpoint refused"));
    QCOMPARE(model.data(index, Qt::DisplayRole).toString(),
             QStringLiteral("endpoint refused"));
    QCOMPARE(model.data(index, LogModel::CategoryRole).toString(), QStringLiteral("http"));
    QCOMPARE(model.data(index, LogModel::LevelRole).toInt(), int(Diagnostics::Warning));
    QCOMPARE(model.data(index, LogModel::LevelNameRole).toString(), QStringLiteral("WARNING"));
    QVERIFY(!model.data(index, LogModel::TimestampRole).toString().isEmpty());

    // The copy helper renders identity, severity, category, and message.
    const QString text = model.entryText(0);
    QVERIFY(text.contains(QStringLiteral("WARNING")));
    QVERIFY(text.contains(QStringLiteral("http")));
    QVERIFY(text.contains(QStringLiteral("endpoint refused")));

    QVERIFY(model.entryText(-1).isEmpty());
    QVERIFY(model.entryText(5).isEmpty());
}

/*!
 * \brief Verifies that the log drops the oldest entries instead of growing forever.
 */
void LogModelTest::reachingTheCapDropsOldestEntries()
{
    LogModel model(20);
    QCOMPARE(model.maximumEntries(), 20);

    appendEntries(model, Diagnostics::Info, 20);
    QCOMPARE(model.rowCount(), 20);
    QCOMPARE(model.data(model.index(0, 0), LogModel::MessageRole).toString(),
             QStringLiteral("message 0"));

    // The next entry trims a batch from the front rather than a single row.
    model.appendEntry(Diagnostics::Info, QString(), QStringLiteral("overflow"));
    QVERIFY(model.rowCount() < 20);
    QVERIFY(model.rowCount() > 0);
    QVERIFY(model.data(model.index(0, 0), LogModel::MessageRole).toString()
            != QStringLiteral("message 0"));

    // The newest entry is always kept.
    const int last = model.rowCount() - 1;
    QCOMPARE(model.data(model.index(last, 0), LogModel::MessageRole).toString(),
             QStringLiteral("overflow"));

    // A pathologically small cap is raised to a usable minimum.
    LogModel tiny(1);
    QCOMPARE(tiny.maximumEntries(), 10);
}

/*!
 * \brief Verifies that clear() empties the log and reports the change.
 */
void LogModelTest::clearEmptiesTheLog()
{
    LogModel model;
    appendEntries(model, Diagnostics::Info, 3);

    QSignalSpy countSpy(&model, &LogModel::countChanged);
    model.clear();

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(countSpy.count(), 1);

    // Clearing an empty log is a no-op and reports nothing.
    model.clear();
    QCOMPARE(countSpy.count(), 1);
}

/*!
 * \brief Verifies that the default logging category is not shown as a category.
 */
void LogModelTest::defaultCategoryIsNotShown()
{
    LogModel model;

    // Qt names the unnamed category "default"; repeating it on every line would
    // be noise, so it is stored as no category at all.
    model.appendEntry(Diagnostics::Info, QStringLiteral("default"),
                      QStringLiteral("plain message"));

    QCOMPARE(model.data(model.index(0, 0), LogModel::CategoryRole).toString(), QString());
    QVERIFY(!model.entryText(0).contains(QStringLiteral("default")));
}

/*!
 * \brief Verifies that the severity filter hides entries below the chosen level.
 */
void LogModelTest::severityFilterHidesLowerLevels()
{
    LogModel model;
    LogFilterModel proxy;
    proxy.setSourceModel(&model);

    model.appendEntry(Diagnostics::Debug, QString(), QStringLiteral("noise"));
    model.appendEntry(Diagnostics::Info, QString(), QStringLiteral("progress"));
    model.appendEntry(Diagnostics::Warning, QString(), QStringLiteral("odd"));
    model.appendEntry(Diagnostics::Error, QString(), QStringLiteral("failed"));

    // Info is the default, so developer detail stays hidden.
    QCOMPARE(proxy.minimumLevel(), int(Diagnostics::Info));
    QCOMPARE(proxy.rowCount(), 3);

    QSignalSpy levelSpy(&proxy, &LogFilterModel::minimumLevelChanged);

    proxy.setMinimumLevel(Diagnostics::Error);
    QCOMPARE(levelSpy.count(), 1);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.count(), 1);

    proxy.setMinimumLevel(Diagnostics::Debug);
    QCOMPARE(proxy.rowCount(), 4);

    // Out-of-range levels are clamped rather than hiding everything.
    proxy.setMinimumLevel(99);
    QCOMPARE(proxy.minimumLevel(), int(Diagnostics::Error));
    proxy.setMinimumLevel(-5);
    QCOMPARE(proxy.minimumLevel(), int(Diagnostics::Debug));
}

/*!
 * \brief Verifies that the text filter matches the message and the category.
 */
void LogModelTest::textFilterMatchesMessageAndCategory()
{
    LogModel model;
    LogFilterModel proxy;
    proxy.setSourceModel(&model);

    model.appendEntry(Diagnostics::Info, QStringLiteral("http"),
                      QStringLiteral("browse finished"));
    model.appendEntry(Diagnostics::Info, QStringLiteral("project"),
                      QStringLiteral("saved to disk"));

    proxy.setFilterText(QStringLiteral("BROWSE"));
    QCOMPARE(proxy.rowCount(), 1);

    // The category is searched as well, so a subsystem can be isolated.
    proxy.setFilterText(QStringLiteral("project"));
    QCOMPARE(proxy.rowCount(), 1);

    proxy.setFilterText(QStringLiteral("nothing"));
    QCOMPARE(proxy.rowCount(), 0);

    proxy.setFilterText(QString());
    QCOMPARE(proxy.rowCount(), 2);
}

/*!
 * \brief Verifies that the copy helper renders only the entries currently shown.
 */
void LogModelTest::visibleTextRendersShownEntriesOnly()
{
    LogModel model;
    LogFilterModel proxy;
    proxy.setSourceModel(&model);

    model.appendEntry(Diagnostics::Info, QString(), QStringLiteral("kept"));
    model.appendEntry(Diagnostics::Error, QString(), QStringLiteral("failure"));

    proxy.setMinimumLevel(Diagnostics::Error);

    const QString text = proxy.visibleText();
    QVERIFY(text.contains(QStringLiteral("failure")));
    QVERIFY(!text.contains(QStringLiteral("kept")));
    QCOMPARE(text.count(QLatin1Char('\n')), 0);

    proxy.setMinimumLevel(Diagnostics::Info);
    QCOMPARE(proxy.visibleText().count(QLatin1Char('\n')), 1);

    // Without a source model there is nothing to render.
    LogFilterModel detached;
    QVERIFY(detached.visibleText().isEmpty());
}

QTEST_MAIN(LogModelTest)

#include "tst_logmodel.moc"
