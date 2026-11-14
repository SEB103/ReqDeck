// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "qmlapi/helplibrary.h"
#include "qmlapi/helpmanual.h"

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

/*!
 * \internal
 * \brief Returns the languages that have a help file in the source tree.
 */
static QStringList helpLanguages()
{
    QStringList languages;
    for (const QString &language : {QStringLiteral("en"), QStringLiteral("de")}) {
        if (QFileInfo::exists(QStringLiteral(REQDECK_HELP_DIR "/help.") + language + QStringLiteral(".json")))
            languages.append(language);
    }
    return languages;
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

    /*! Verifies that a broken file leaves the application working. */
    void brokenFileGivesEmptyTopics();

    /*! Verifies that the language switches, and falls back to English without a file. */
    void switchesLanguage();

    /*! Verifies that every language has the same topics and every heading of the panel. */
    void languagesHaveTheSameTopics();

    /*! Verifies that the Help menu links are web addresses. */
    void linksAreWebAddresses();

    /*! Verifies that no topic id appears twice in a help file, since JSON keeps only one. */
    void topicIdsAreUnique();

    /*! Verifies that every topic names a page and a section of the manual in its language. */
    void topicsNameManualSections();
};

void HelpLibraryTest::loadsTopics()
{
    QTemporaryDir dir;
    writeHelp(dir, QStringLiteral("en"), R"({"schema":1,"topics":{"request.url":{"title":"Address","what":"Where it goes.","why":"Names the server.","example":"https://h","next":"Press Send.","more":"Requests#method-and-address"}}})");

    const HelpLibrary help(QStringLiteral("en"), dir.path());

    const QVariantMap topic = help.topic(QStringLiteral("request.url"));
    QCOMPARE(topic.value(QStringLiteral("title")).toString(), QStringLiteral("Address"));
    QCOMPARE(topic.value(QStringLiteral("what")).toString(), QStringLiteral("Where it goes."));
    QCOMPARE(topic.value(QStringLiteral("why")).toString(), QStringLiteral("Names the server."));
    QCOMPARE(topic.value(QStringLiteral("example")).toString(), QStringLiteral("https://h"));
    QCOMPARE(topic.value(QStringLiteral("next")).toString(), QStringLiteral("Press Send."));
    QCOMPARE(topic.value(QStringLiteral("more")).toString(), QStringLiteral("Requests#method-and-address"));
    QVERIFY(help.hasTopic(QStringLiteral("request.url")));
    QCOMPARE(help.topicIds(), QStringList({QStringLiteral("request.url")}));
}

void HelpLibraryTest::unknownTopicIsEmpty()
{
    QTemporaryDir dir;
    writeHelp(dir, QStringLiteral("en"), R"({"schema":1,"topics":{}})");

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

void HelpLibraryTest::switchesLanguage()
{
    QTemporaryDir dir;
    writeHelp(dir, QStringLiteral("en"), R"({"topics":{"start":{"title":"Start"}}})");
    HelpLibrary help(QStringLiteral("de"), dir.path());
    QCOMPARE(help.language(), QStringLiteral("en"));
    QSignalSpy changed(&help, &HelpLibrary::languageChanged);

    help.setLanguage(QStringLiteral("de"));
    QCOMPARE(help.language(), QStringLiteral("en"));
    QCOMPARE(changed.count(), 0);

    writeHelp(dir, QStringLiteral("de"), R"({"topics":{"start":{"title":"Anfang"}}})");
    help.setLanguage(QStringLiteral("de"));
    QCOMPARE(help.language(), QStringLiteral("de"));
    QCOMPARE(help.topic(QStringLiteral("start")).value(QStringLiteral("title")), QStringLiteral("Anfang"));
    QCOMPARE(changed.count(), 1);
}

void HelpLibraryTest::languagesHaveTheSameTopics()
{
    const HelpLibrary english(QStringLiteral("en"), QStringLiteral(REQDECK_HELP_DIR));
    QVERIFY(english.topicIds().size() > 40);
    for (const QString &language : helpLanguages()) {
        const HelpLibrary help(language, QStringLiteral(REQDECK_HELP_DIR));
        QCOMPARE(help.language(), language);
        QCOMPARE(help.topicIds(), english.topicIds());
        for (const QString &key : {QStringLiteral("why"), QStringLiteral("example"), QStringLiteral("next"),
                                   QStringLiteral("more"), QStringLiteral("chapter"), QStringLiteral("close")}) {
            QVERIFY2(!help.labels().value(key).toString().isEmpty(), qPrintable(language + u' ' + key));
        }
        for (const QString &id : english.topicIds()) {
            for (const QString &part : {QStringLiteral("title"), QStringLiteral("what"), QStringLiteral("more")})
                QVERIFY2(!help.topic(id).value(part).toString().isEmpty(), qPrintable(language + u' ' + id + u' ' + part));
        }
    }
}

void HelpLibraryTest::linksAreWebAddresses()
{
    for (const QString &language : helpLanguages()) {
        const HelpLibrary help(language, QStringLiteral(REQDECK_HELP_DIR));
        for (const QString &key : {QStringLiteral("practice"), QStringLiteral("statusCodes")}) {
            const QUrl url(help.links().value(key).toString());
            QVERIFY2(url.isValid() && url.scheme() == QStringLiteral("https"), qPrintable(language + u' ' + key));
        }
    }
}

void HelpLibraryTest::topicIdsAreUnique()
{
    for (const QString &language : helpLanguages()) {
        QFile file(QDir(QStringLiteral(REQDECK_HELP_DIR)).filePath(QStringLiteral("help.%1.json").arg(language)));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(file.readAll());
        const QJsonObject topics = QJsonDocument::fromJson(text.toUtf8()).object().value(QStringLiteral("topics")).toObject();
        for (const QString &key : topics.keys()) {
            const QString declaration = QStringLiteral("\n        \"%1\": {").arg(key);
            QVERIFY2(text.count(declaration) == 1, qPrintable(language + u' ' + key));
        }
    }
}

void HelpLibraryTest::topicsNameManualSections()
{
    static const QRegularExpression headingLine(QStringLiteral("^#+ (.+)$"), QRegularExpression::MultilineOption);
    for (const QString &language : helpLanguages()) {
        const QDir manual(QStringLiteral(REQDECK_MANUAL_DIR "/") + language);
        const HelpLibrary help(language, QStringLiteral(REQDECK_HELP_DIR));
        for (const QString &id : help.topicIds()) {
            const QString more = help.topic(id).value(QStringLiteral("more")).toString();
            const QString page = more.section(u'#', 0, 0);
            const QString anchor = more.section(u'#', 1);
            QFile file(manual.filePath(page + QStringLiteral(".md")));
            QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(language + u' ' + id + u' ' + page));
            if (anchor.isEmpty())
                continue;
            QStringList slugs;
            for (const QRegularExpressionMatch &match : headingLine.globalMatch(QString::fromUtf8(file.readAll())))
                slugs.append(HelpManual::slug(match.captured(1)));
            QVERIFY2(slugs.contains(anchor), qPrintable(language + u' ' + id + u' ' + more));
        }
    }
}

QTEST_GUILESS_MAIN(HelpLibraryTest)

#include "tst_helplibrary.moc"
