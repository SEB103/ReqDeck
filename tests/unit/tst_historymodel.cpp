// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "core/history/historystore.h"
#include "core/http/responsedata.h"
#include "qmlapi/historymodel.h"

/*!
 * \internal
 * \brief Returns a POST request to \a url.
 */
static RequestSpec makeRequest(const QString &url)
{
    RequestSpec request;
    request.method = HttpMethod::Post;
    request.url = url;
    return request;
}

/*!
 * \internal
 * \brief Returns a response with \a statusCode.
 */
static ResponseData makeResponse(int statusCode)
{
    ResponseData response;
    response.statusCode = statusCode;
    response.elapsedMs = 12;
    return response;
}

/*!
 * \internal
 * \brief Returns a response that failed with \a error.
 */
static ResponseData makeFailure(ResponseError error)
{
    ResponseData response;
    response.error = error;
    response.errorString = QStringLiteral("It failed");
    return response;
}

/*! Verifies the history list over a real store. */
class HistoryModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that entries from an earlier run appear. */
    void loadsExistingEntries();

    /*! Verifies that the newest request comes first, with all its roles. */
    void recordPrependsRow();

    /*! Verifies that a new row leaves the others where they are. */
    void recordDoesNotResetModel();

    /*! Verifies that a full list swaps its oldest row for the new one. */
    void recordAtCapRemovesOldestRow();

    /*! Verifies that a request without an answer is kept. */
    void recordKeepsFailures();

    /*! Verifies that a request the user cancelled is not kept. */
    void recordSkipsAborted();

    /*! Verifies that literal credentials are gone and references stay. */
    void recordRedactsCredentials();

    /*! Verifies the limit of the store. */
    void capsAt200Rows();

    /*! Verifies that clearing removes rows and file. */
    void clearEmptiesModelAndFile();

    /*! Verifies a damaged file. */
    void corruptFileStartsEmptyAndReports();

    /*! Verifies that a failing file does not lose the row and reports once. */
    void unwritableFileKeepsResponse();

    /*! Verifies the name of a request made from an entry. */
    void displayNameNamesTheRequest();
};

void HistoryModelTest::loadsExistingEntries()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("history.json"));
    HistoryEntry entry;
    entry.request = makeRequest(QStringLiteral("http://earlier"));
    QVERIFY(HistoryStore(path).add(entry));

    HistoryModel model(path);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy failed(&model, &HistoryModel::storageFailed);
    model.load();

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), HistoryModel::UrlRole).toString(),
             QStringLiteral("http://earlier"));
    QCOMPARE(failed.count(), 0);
}

void HistoryModelTest::recordPrependsRow()
{
    QTemporaryDir dir;
    HistoryModel model(dir.filePath(QStringLiteral("history.json")));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

    model.record(makeRequest(QStringLiteral("http://first")), makeResponse(200));
    model.record(makeRequest(QStringLiteral("http://second")), makeResponse(404));

    QCOMPARE(model.rowCount(), 2);
    const QModelIndex newest = model.index(0);
    QCOMPARE(model.data(newest, HistoryModel::UrlRole).toString(), QStringLiteral("http://second"));
    QCOMPARE(model.data(newest, HistoryModel::MethodRole).toString(), QStringLiteral("POST"));
    QCOMPARE(model.data(newest, HistoryModel::StatusCodeRole).toInt(), 404);
    QCOMPARE(model.data(newest, HistoryModel::ElapsedRole).toLongLong(), qint64(12));
    QVERIFY(!model.data(newest, HistoryModel::IdRole).toString().isEmpty());
    QCOMPARE(model.data(newest, HistoryModel::TimeRole).toString().size(), 19);
    QVERIFY(model.roleNames().values().contains("timeText"));
}

void HistoryModelTest::recordDoesNotResetModel()
{
    QTemporaryDir dir;
    HistoryModel model(dir.filePath(QStringLiteral("history.json")));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.record(makeRequest(QStringLiteral("http://first")), makeResponse(200));
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

    model.record(makeRequest(QStringLiteral("http://second")), makeResponse(200));

    QCOMPARE(reset.count(), 0);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(inserted.at(0).at(1).toInt(), 0);
}

void HistoryModelTest::recordAtCapRemovesOldestRow()
{
    QTemporaryDir dir;
    HistoryModel model(dir.filePath(QStringLiteral("history.json")));
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    for (int i = 0; i < HistoryStore::kMaxEntries; ++i)
        model.record(makeRequest(QString::number(i)), makeResponse(200));
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

    model.record(makeRequest(QStringLiteral("newest")), makeResponse(200));

    QCOMPARE(reset.count(), 0);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(model.rowCount(), HistoryStore::kMaxEntries);
    QCOMPARE(model.data(model.index(0), HistoryModel::UrlRole).toString(), QStringLiteral("newest"));
    QCOMPARE(model.data(model.index(HistoryStore::kMaxEntries - 1), HistoryModel::UrlRole).toString(),
             QStringLiteral("1"));
}

void HistoryModelTest::recordKeepsFailures()
{
    QTemporaryDir dir;
    HistoryModel model(dir.filePath(QStringLiteral("history.json")));

    model.record(makeRequest(QStringLiteral("http://x")), makeFailure(ResponseError::Timeout));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), HistoryModel::StatusCodeRole).toInt(), 0);
    QCOMPARE(model.data(model.index(0), HistoryModel::ErrorRole).toString(),
             QStringLiteral("It failed"));
}

void HistoryModelTest::recordSkipsAborted()
{
    QTemporaryDir dir;
    HistoryModel model(dir.filePath(QStringLiteral("history.json")));

    model.record(makeRequest(QStringLiteral("http://x")), makeFailure(ResponseError::Aborted));

    QCOMPARE(model.rowCount(), 0);
}

void HistoryModelTest::recordRedactsCredentials()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("history.json"));
    HistoryModel model(path);
    RequestSpec request = makeRequest(QStringLiteral("http://x"));
    request.auth.type = AuthType::Basic;
    request.auth.username = QStringLiteral("{{user}}");
    request.auth.password = QStringLiteral("hunter2-literal");

    model.record(request, makeResponse(200));

    QCOMPARE(model.requestAt(0).auth.username, QStringLiteral("{{user}}"));
    QVERIFY(model.requestAt(0).auth.password.isEmpty());
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(!file.readAll().contains("hunter2-literal"));
}

void HistoryModelTest::capsAt200Rows()
{
    QTemporaryDir dir;
    HistoryModel model(dir.filePath(QStringLiteral("history.json")));

    for (int i = 0; i < HistoryStore::kMaxEntries + 1; ++i)
        model.record(makeRequest(QString::number(i)), makeResponse(200));

    QCOMPARE(model.rowCount(), HistoryStore::kMaxEntries);
    QCOMPARE(model.data(model.index(0), HistoryModel::UrlRole).toString(),
             QString::number(HistoryStore::kMaxEntries));
}

void HistoryModelTest::clearEmptiesModelAndFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("history.json"));
    HistoryModel model(path);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.record(makeRequest(QStringLiteral("http://x")), makeResponse(200));
    QVERIFY(QFile::exists(path));

    model.clear();

    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!QFile::exists(path));
}

void HistoryModelTest::corruptFileStartsEmptyAndReports()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("history.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not json");
    file.close();

    HistoryModel model(path);
    QSignalSpy failed(&model, &HistoryModel::storageFailed);
    model.load();

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(failed.count(), 1);
    QVERIFY(QFile::exists(path + QStringLiteral(".corrupt")));
}

void HistoryModelTest::unwritableFileKeepsResponse()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("history.json"));
    QVERIFY(QDir().mkpath(path));
    HistoryModel model(path);
    QSignalSpy failed(&model, &HistoryModel::storageFailed);

    model.record(makeRequest(QStringLiteral("http://x")), makeResponse(200));
    model.record(makeRequest(QStringLiteral("http://y")), makeResponse(200));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(failed.count(), 1);

    QVERIFY(QDir().rmdir(path));
    model.record(makeRequest(QStringLiteral("http://z")), makeResponse(200));
    QVERIFY(QFile::exists(path));
    QCOMPARE(failed.count(), 1);
}

void HistoryModelTest::displayNameNamesTheRequest()
{
    QTemporaryDir dir;
    HistoryModel model(dir.filePath(QStringLiteral("history.json")));

    model.record(makeRequest(QStringLiteral("http://x")), makeResponse(200));

    QCOMPARE(model.displayName(0), QStringLiteral("POST http://x"));
    QVERIFY(model.displayName(1).isEmpty());
}

QTEST_MAIN(HistoryModelTest)

#include "tst_historymodel.moc"
