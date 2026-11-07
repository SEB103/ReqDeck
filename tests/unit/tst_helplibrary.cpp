// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTemporaryDir>
#include <QtTest>

#include "qmlapi/helplibrary.h"

/*!
 * \internal
 * \brief Writes \a content as help.<language>.json into \a dir.
 */
static void writeHelp(const QTemporaryDir &dir, const QString &language, const QByteArray &content)
{
    QFile file(dir.filePath(QStringLiteral("help.%1.json").arg(language)));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

/*! Verifies the help topics shown by F1. */
class HelpLibraryTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that a topic carries all its parts. */
    void loadsTopics();

    /*! Verifies that an unknown id gives an empty topic. */
    void unknownTopicIsEmpty();

    /*! Verifies that a missing file leaves the application working. */
    void missingFileGivesEmptyTopics();

    /*! Verifies that a damaged file leaves the application working. */
    void brokenFileGivesEmptyTopics();

    /*! Verifies the choice of language. */
    void languageForRussianAndOthers();

    /*! Verifies that both shipped languages explain the same things. */
    void languagesHaveTheSameTopics();

    /*! Verifies that each topic points to an existing chapter of the guide. */
    void topicsPointToChapters();

    /*! Verifies the headings of the panel in the language of the topics. */
    void labelsFollowLanguage();

    /*! Verifies where the guide is looked for. */
    void guidePathFollowsLanguage();
};

void HelpLibraryTest::loadsTopics()
{
    QTemporaryDir dir;
    writeHelp(dir, QStringLiteral("en"), R"({"schema":1,"topics":{"request.url":{"title":"Address","what":"Where it goes.","why":"Names the server.","example":"https://h","next":"Press Send.","chapter":"first-request"}},"chapters":[]})");

    const HelpLibrary help(QStringLiteral("en"), dir.path());

    const QVariantMap topic = help.topic(QStringLiteral("request.url"));
    QCOMPARE(topic.value(QStringLiteral("title")).toString(), QStringLiteral("Address"));
    QCOMPARE(topic.value(QStringLiteral("what")).toString(), QStringLiteral("Where it goes."));
    QCOMPARE(topic.value(QStringLiteral("why")).toString(), QStringLiteral("Names the server."));
    QCOMPARE(topic.value(QStringLiteral("example")).toString(), QStringLiteral("https://h"));
    QCOMPARE(topic.value(QStringLiteral("next")).toString(), QStringLiteral("Press Send."));
    QCOMPARE(topic.value(QStringLiteral("chapter")).toString(), QStringLiteral("first-request"));
    QVERIFY(help.hasTopic(QStringLiteral("request.url")));
    QCOMPARE(help.topicIds(), QStringList({QStringLiteral("request.url")}));
}

void HelpLibraryTest::unknownTopicIsEmpty()
{
    QTemporaryDir dir;
    writeHelp(dir, QStringLiteral("en"), R"({"schema":1,"topics":{},"chapters":[]})");

    const HelpLibrary help(QStringLiteral("en"), dir.path());

    QVERIFY(help.topic(QStringLiteral("nothing")).isEmpty());
    QVERIFY(!help.hasTopic(QStringLiteral("nothing")));
}

void HelpLibraryTest::missingFileGivesEmptyTopics()
{
    QTemporaryDir dir;

    const HelpLibrary help(QStringLiteral("en"), dir.path());

    QVERIFY(help.topicIds().isEmpty());
    QVERIFY(help.topic(QStringLiteral("start")).isEmpty());
}

void HelpLibraryTest::brokenFileGivesEmptyTopics()
{
    QTemporaryDir dir;
    writeHelp(dir, QStringLiteral("en"), "{not json");

    const HelpLibrary help(QStringLiteral("en"), dir.path());

    QVERIFY(help.topicIds().isEmpty());
}

void HelpLibraryTest::languageForRussianAndOthers()
{
    QCOMPARE(HelpLibrary::languageFor(QLocale(QLocale::Russian, QLocale::Russia)), QStringLiteral("ru"));
    QCOMPARE(HelpLibrary::languageFor(QLocale(QLocale::Russian, QLocale::Kazakhstan)), QStringLiteral("ru"));
    QCOMPARE(HelpLibrary::languageFor(QLocale(QLocale::German, QLocale::Germany)), QStringLiteral("en"));
    QCOMPARE(HelpLibrary::languageFor(QLocale(QLocale::English, QLocale::UnitedStates)), QStringLiteral("en"));
}

void HelpLibraryTest::languagesHaveTheSameTopics()
{
    const HelpLibrary english(QStringLiteral("en"), QStringLiteral(REQDECK_HELP_DIR));
    const HelpLibrary russian(QStringLiteral("ru"), QStringLiteral(REQDECK_HELP_DIR));

    QVERIFY(english.topicIds().size() > 40);
    QCOMPARE(russian.topicIds(), english.topicIds());
    QCOMPARE(russian.chapterIds(), english.chapterIds());
    for (const QString &id : english.topicIds()) {
        for (const QString &part : {QStringLiteral("title"), QStringLiteral("what"), QStringLiteral("why"),
                                    QStringLiteral("example"), QStringLiteral("next")}) {
            QVERIFY2(!english.topic(id).value(part).toString().isEmpty(), qPrintable(id + u' ' + part));
            QVERIFY2(!russian.topic(id).value(part).toString().isEmpty(), qPrintable(id + u' ' + part));
        }
    }
}

void HelpLibraryTest::topicsPointToChapters()
{
    const HelpLibrary english(QStringLiteral("en"), QStringLiteral(REQDECK_HELP_DIR));

    for (const QString &id : english.topicIds()) {
        const QString chapter = english.topic(id).value(QStringLiteral("chapter")).toString();
        QVERIFY2(english.chapterIds().contains(chapter), qPrintable(id + u' ' + chapter));
    }
}

void HelpLibraryTest::labelsFollowLanguage()
{
    const HelpLibrary russian(QStringLiteral("ru"), QStringLiteral(REQDECK_HELP_DIR));
    const HelpLibrary english(QStringLiteral("en"), QStringLiteral(REQDECK_HELP_DIR));

    for (const QString &key : {QStringLiteral("why"), QStringLiteral("example"), QStringLiteral("next"),
                               QStringLiteral("more"), QStringLiteral("close")}) {
        QVERIFY2(!russian.labels().value(key).toString().isEmpty(), qPrintable(key));
        QVERIFY2(!english.labels().value(key).toString().isEmpty(), qPrintable(key));
    }
    QCOMPARE(english.labels().value(QStringLiteral("why")).toString(), QStringLiteral("Why"));
    QCOMPARE(russian.labels().value(QStringLiteral("why")).toString(), QStringLiteral("Зачем"));
}

void HelpLibraryTest::guidePathFollowsLanguage()
{
    QTemporaryDir dir;
    const HelpLibrary russian(QStringLiteral("ru"), dir.path());

    QVERIFY(russian.guidePath().endsWith(QStringLiteral("/help/ReqDeck-Guide.ru.pdf")));
    QCOMPARE(russian.language(), QStringLiteral("ru"));
}

QTEST_GUILESS_MAIN(HelpLibraryTest)

#include "tst_helplibrary.moc"
