// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest>

#include "qmlapi/formpartmodel.h"

/*! Verifies the editable table of multipart parts. */
class FormPartModelTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies setRows(), the roles, and that it emits no edit. */
    void showsRowsWithoutEdit();

    /*! Verifies that setField() changes one role and emits one edit, none for no change. */
    void editsFields();

    /*! Verifies adding a text row, adding a file from a URL, and removing. */
    void addsAndRemoves();
};

void FormPartModelTest::showsRowsWithoutEdit()
{
    FormPartModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy edited(&model, &FormPartModel::edited);
    model.setRows({{QStringLiteral("a"), QStringLiteral("1"), true, false, QString()},
                   {QStringLiteral("f"), QStringLiteral("C:/x.png"), false, true, QStringLiteral("image/png")}});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(edited.count(), 0);
    const QModelIndex file = model.index(1);
    QCOMPARE(file.data(FormPartModel::ValueRole).toString(), QStringLiteral("C:/x.png"));
    QVERIFY(file.data(FormPartModel::IsFileRole).toBool());
    QVERIFY(!file.data(FormPartModel::EnabledRole).toBool());
    QCOMPARE(file.data(FormPartModel::ContentTypeRole).toString(), QStringLiteral("image/png"));
    QCOMPARE(model.roleNames().value(FormPartModel::IsFileRole), QByteArray("isFile"));
}

void FormPartModelTest::editsFields()
{
    FormPartModel model;
    model.setRows({{QStringLiteral("a"), QStringLiteral("1"), true, false, QString()}});
    QSignalSpy edited(&model, &FormPartModel::edited);

    QVERIFY(model.setField(0, FormPartModel::IsFileRole, true));
    QVERIFY(model.setField(0, FormPartModel::KeyRole, QStringLiteral("b")));
    QVERIFY(model.setField(0, FormPartModel::KeyRole, QStringLiteral("b")));
    QVERIFY(!model.setField(4, FormPartModel::KeyRole, QStringLiteral("x")));
    QVERIFY(!model.setField(0, Qt::DisplayRole, QStringLiteral("x")));

    QCOMPARE(edited.count(), 2);
    QVERIFY(model.rows().at(0).isFile);
    QCOMPARE(model.rows().at(0).key, QStringLiteral("b"));
}

void FormPartModelTest::addsAndRemoves()
{
    FormPartModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QSignalSpy edited(&model, &FormPartModel::edited);

    model.addRow();
    model.addFile(QUrl::fromLocalFile(QStringLiteral("C:/Pictures/beach photo.jpg")).toString());
    model.addFile(QString());

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.rows().at(1).key, QStringLiteral("beach photo"));
    QCOMPARE(model.rows().at(1).value, QStringLiteral("C:/Pictures/beach photo.jpg"));
    QVERIFY(model.rows().at(1).isFile);

    model.removeRow(0);
    model.removeRow(7);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(edited.count(), 3);
}

QTEST_GUILESS_MAIN(FormPartModelTest)

#include "tst_formpartmodel.moc"
