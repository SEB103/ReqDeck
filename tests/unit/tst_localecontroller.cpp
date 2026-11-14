// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "localecontroller.h"

/*! Verifies the choice and the switch of the interface language. */
class LocaleControllerTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that English and German are offered, with their short names. */
    void listsEnglishAndGerman();

    /*! Verifies how the start language is chosen. */
    void resolvesStartLanguage_data();
    void resolvesStartLanguage();

    /*! Verifies that a switch applies, is announced once, and is saved. */
    void switchesAndSaves();
};

void LocaleControllerTest::listsEnglishAndGerman()
{
    const LocaleController controller(nullptr);
    const QVariantList rows = controller.availableLanguages();

    QCOMPARE(rows.size(), 2);
    const QVariantMap english = rows.at(0).toMap();
    const QVariantMap german = rows.at(1).toMap();
    QCOMPARE(english.value(QStringLiteral("code")), QStringLiteral("en_GB"));
    QCOMPARE(english.value(QStringLiteral("name")), QStringLiteral("English"));
    QCOMPARE(english.value(QStringLiteral("shortName")), QStringLiteral("EN"));
    QCOMPARE(german.value(QStringLiteral("code")), QStringLiteral("de_DE"));
    QCOMPARE(german.value(QStringLiteral("name")), QStringLiteral("Deutsch"));
    QCOMPARE(german.value(QStringLiteral("shortName")), QStringLiteral("DE"));
}

void LocaleControllerTest::resolvesStartLanguage_data()
{
    QTest::addColumn<QStringList>("uiLanguages");
    QTest::addColumn<QString>("saved");
    QTest::addColumn<QString>("expected");

    QTest::newRow("German Windows") << QStringList {QStringLiteral("de-DE")} << QString() << QStringLiteral("de_DE");
    QTest::newRow("Swiss German") << QStringList {QStringLiteral("de-CH"), QStringLiteral("en-US")} << QString()
                                  << QStringLiteral("de_DE");
    QTest::newRow("English Windows") << QStringList {QStringLiteral("en-US"), QStringLiteral("de-DE")} << QString()
                                     << QStringLiteral("en_GB");
    QTest::newRow("Russian Windows") << QStringList {QStringLiteral("ru-RU"), QStringLiteral("de-DE")} << QString()
                                     << QStringLiteral("en_GB");
    QTest::newRow("no languages") << QStringList() << QString() << QStringLiteral("en_GB");
    QTest::newRow("saved wins") << QStringList {QStringLiteral("de-DE")} << QStringLiteral("en_GB")
                                << QStringLiteral("en_GB");
    QTest::newRow("saved German") << QStringList {QStringLiteral("en-US")} << QStringLiteral("de_DE")
                                  << QStringLiteral("de_DE");
    QTest::newRow("unknown saved") << QStringList {QStringLiteral("en-US")} << QStringLiteral("xx_XX")
                                   << QStringLiteral("en_GB");
}

void LocaleControllerTest::resolvesStartLanguage()
{
    QFETCH(QStringList, uiLanguages);
    QFETCH(QString, saved);
    QFETCH(QString, expected);

    const LocaleController controller(nullptr);
    QCOMPARE(controller.resolveCode(uiLanguages, saved), expected);
}

void LocaleControllerTest::switchesAndSaves()
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    LocaleController controller(&settings);
    controller.setLanguage(QStringLiteral("en_GB"));
    QSignalSpy changed(&controller, &LocaleController::currentLanguageChanged);

    controller.setLanguage(QStringLiteral("de_DE"));

    QCOMPARE(controller.currentLanguage(), QStringLiteral("de_DE"));
    QCOMPARE(controller.currentShortName(), QStringLiteral("DE"));
    QCOMPARE(controller.currentLanguageName(), QStringLiteral("Deutsch"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(settings.value(QStringLiteral("ui/language")).toString(), QStringLiteral("de_DE"));

    controller.setLanguage(QStringLiteral("de_DE"));
    controller.setLanguage(QStringLiteral("fr_FR"));
    QCOMPARE(controller.currentLanguage(), QStringLiteral("de_DE"));
    QCOMPARE(changed.count(), 1);
}

QTEST_GUILESS_MAIN(LocaleControllerTest)

#include "tst_localecontroller.moc"
