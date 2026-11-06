// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QtTest>

#include "qmlapi/environmentvariablesmodel.h"

/*!
 * \internal
 * \brief Returns two rows, the second one disabled and secret.
 */
static QList<EnvironmentVariable> makeRows()
{
    return {{QStringLiteral("a"), QStringLiteral("1"), true, false},
            {QStringLiteral("b"), QStringLiteral("2"), false, true}};
}

/*! Verifies the editable table of environment variables. */
class EnvironmentVariablesModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that loading rows is not reported as an edit. */
    void setRowsDoesNotEmitEdited();

    /*! Verifies that a changed role reaches the rows and reports an edit. */
    void setDataEmitsEditedOnlyOnChange_data();
    /*! Runs one role case. */
    void setDataEmitsEditedOnlyOnChange();

    /*! Verifies the row added by addRow(). */
    void addRowAppendsEmptyEnabledNotSecret();

    /*! Verifies that removing a row reports an edit. */
    void removeRowDeletesAndEmitsEdited();

    /*! Verifies that an invalid row number changes nothing. */
    void removeRowIgnoresInvalidRow();

    /*! Verifies the names BsKeyValueTable uses. */
    void roleNamesMatchTable();
};

void EnvironmentVariablesModelTest::setRowsDoesNotEmitEdited()
{
    EnvironmentVariablesModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy edited(&model, &EnvironmentVariablesModel::edited);

    model.setRows(makeRows());

    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.rows() == makeRows());
    QCOMPARE(edited.count(), 0);
}

void EnvironmentVariablesModelTest::setDataEmitsEditedOnlyOnChange_data()
{
    QTest::addColumn<int>("role");
    QTest::addColumn<QVariant>("same");
    QTest::addColumn<QVariant>("changed");

    QTest::newRow("key") << int(EnvironmentVariablesModel::KeyRole)
                         << QVariant(QStringLiteral("a")) << QVariant(QStringLiteral("x"));
    QTest::newRow("value") << int(EnvironmentVariablesModel::ValueRole)
                           << QVariant(QStringLiteral("1")) << QVariant(QStringLiteral("9"));
    QTest::newRow("enabled") << int(EnvironmentVariablesModel::EnabledRole) << QVariant(true)
                             << QVariant(false);
    QTest::newRow("secret") << int(EnvironmentVariablesModel::SecretRole) << QVariant(false)
                            << QVariant(true);
}

void EnvironmentVariablesModelTest::setDataEmitsEditedOnlyOnChange()
{
    QFETCH(int, role);
    QFETCH(QVariant, same);
    QFETCH(QVariant, changed);

    EnvironmentVariablesModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setRows(makeRows());
    QSignalSpy edited(&model, &EnvironmentVariablesModel::edited);
    const QModelIndex first = model.index(0);

    QVERIFY(model.setData(first, same, role));
    QCOMPARE(edited.count(), 0);

    QVERIFY(model.setData(first, changed, role));
    QCOMPARE(edited.count(), 1);
    QCOMPARE(model.data(first, role), changed);
}

void EnvironmentVariablesModelTest::addRowAppendsEmptyEnabledNotSecret()
{
    EnvironmentVariablesModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy edited(&model, &EnvironmentVariablesModel::edited);

    model.addRow();

    QCOMPARE(model.rowCount(), 1);
    QVERIFY(model.rows().constFirst() == EnvironmentVariable {});
    QVERIFY(model.rows().constFirst().enabled);
    QVERIFY(!model.rows().constFirst().secret);
    QCOMPARE(edited.count(), 1);
}

void EnvironmentVariablesModelTest::removeRowDeletesAndEmitsEdited()
{
    EnvironmentVariablesModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setRows(makeRows());
    QSignalSpy edited(&model, &EnvironmentVariablesModel::edited);

    model.removeRow(0);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rows().constFirst().key, QStringLiteral("b"));
    QCOMPARE(edited.count(), 1);
}

void EnvironmentVariablesModelTest::removeRowIgnoresInvalidRow()
{
    EnvironmentVariablesModel model;
    model.setRows(makeRows());
    QSignalSpy edited(&model, &EnvironmentVariablesModel::edited);

    model.removeRow(-1);
    model.removeRow(2);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(edited.count(), 0);
}

void EnvironmentVariablesModelTest::roleNamesMatchTable()
{
    const QHash<int, QByteArray> names = EnvironmentVariablesModel().roleNames();

    QCOMPARE(names.value(EnvironmentVariablesModel::KeyRole), QByteArrayLiteral("key"));
    QCOMPARE(names.value(EnvironmentVariablesModel::ValueRole), QByteArrayLiteral("value"));
    QCOMPARE(names.value(EnvironmentVariablesModel::EnabledRole), QByteArrayLiteral("enabled"));
    QCOMPARE(names.value(EnvironmentVariablesModel::SecretRole), QByteArrayLiteral("secret"));
}

QTEST_MAIN(EnvironmentVariablesModelTest)

#include "tst_environmentvariablesmodel.moc"
