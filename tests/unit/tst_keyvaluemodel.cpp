// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QtTest>

#include "qmlapi/keyvaluemodel.h"

/*!
 * \internal
 * \brief Returns two rows, the second one disabled.
 */
static QList<KeyValue> makeRows()
{
    return {{QStringLiteral("a"), QStringLiteral("1"), true},
            {QStringLiteral("b"), QStringLiteral("2"), false}};
}

/*! Verifies the editable key/value table model. */
class KeyValueModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that loading rows is not reported as an edit. */
    void setRowsResetsWithoutEdited();

    /*! Verifies that a changed role reaches the rows and reports an edit. */
    void setDataChangesRoleAndEmitsEdited_data();
    /*! Runs one role case. */
    void setDataChangesRoleAndEmitsEdited();

    /*! Verifies that writing the same value is no edit. */
    void setDataWithSameValueStaysQuiet();

    /*! Verifies the row added by addRow(). */
    void addRowAppendsEnabledEmptyRow();

    /*! Verifies removing a row. */
    void removeRowDeletesAndEmitsEdited();

    /*! Verifies that an invalid row number changes nothing. */
    void removeRowOutOfRangeIsIgnored();

    /*! Verifies the names QML delegates use. */
    void roleNamesMatchDelegates();
};

void KeyValueModelTest::setRowsResetsWithoutEdited()
{
    KeyValueModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy edited(&model, &KeyValueModel::edited);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    model.setRows(makeRows());

    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.rows() == makeRows());
    QCOMPARE(reset.count(), 1);
    QCOMPARE(edited.count(), 0);
}

void KeyValueModelTest::setDataChangesRoleAndEmitsEdited_data()
{
    QTest::addColumn<int>("role");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("key") << int(KeyValueModel::KeyRole) << QVariant(QStringLiteral("x"));
    QTest::newRow("value") << int(KeyValueModel::ValueRole) << QVariant(QStringLiteral("y"));
    QTest::newRow("enabled") << int(KeyValueModel::EnabledRole) << QVariant(false);
}

void KeyValueModelTest::setDataChangesRoleAndEmitsEdited()
{
    QFETCH(int, role);
    QFETCH(QVariant, value);

    KeyValueModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setRows(makeRows());
    QSignalSpy edited(&model, &KeyValueModel::edited);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    QVERIFY(model.setData(model.index(0), value, role));

    QCOMPARE(model.index(0).data(role), value);
    QCOMPARE(edited.count(), 1);
    QCOMPARE(changed.count(), 1);
    QVERIFY(model.flags(model.index(0)) & Qt::ItemIsEditable);
}

void KeyValueModelTest::setDataWithSameValueStaysQuiet()
{
    KeyValueModel model;
    model.setRows(makeRows());
    QSignalSpy edited(&model, &KeyValueModel::edited);

    QVERIFY(model.setData(model.index(0), QStringLiteral("a"), KeyValueModel::KeyRole));
    QCOMPARE(edited.count(), 0);
}

void KeyValueModelTest::addRowAppendsEnabledEmptyRow()
{
    KeyValueModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setRows(makeRows());
    QSignalSpy edited(&model, &KeyValueModel::edited);

    model.addRow();

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.rows().last(), (KeyValue {QString(), QString(), true}));
    QCOMPARE(edited.count(), 1);
}

void KeyValueModelTest::removeRowDeletesAndEmitsEdited()
{
    KeyValueModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.setRows(makeRows());
    QSignalSpy edited(&model, &KeyValueModel::edited);

    model.removeRow(0);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.rows().first().key, QStringLiteral("b"));
    QCOMPARE(edited.count(), 1);
}

void KeyValueModelTest::removeRowOutOfRangeIsIgnored()
{
    KeyValueModel model;
    model.setRows(makeRows());
    QSignalSpy edited(&model, &KeyValueModel::edited);

    model.removeRow(-1);
    model.removeRow(2);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(edited.count(), 0);
}

void KeyValueModelTest::roleNamesMatchDelegates()
{
    const KeyValueModel model;
    const QHash<int, QByteArray> names = model.roleNames();
    QCOMPARE(names.value(KeyValueModel::KeyRole), QByteArray("key"));
    QCOMPARE(names.value(KeyValueModel::ValueRole), QByteArray("value"));
    QCOMPARE(names.value(KeyValueModel::EnabledRole), QByteArray("enabled"));
}

QTEST_MAIN(KeyValueModelTest)

#include "tst_keyvaluemodel.moc"
