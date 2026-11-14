// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QtTest>

#include "qmlapi/helpmanual.h"

/*! Verifies the user manual shown in the help window. */
class HelpManualTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that the pages come in the order of the manifest. */
    void listsPagesInOrder();

    /*! Verifies that every page of every language loads and stays small. */
    void everyPageLoads();

    /*! Verifies that every link of every page leads somewhere. */
    void everyLinkResolves();

    /*! Verifies that every image of every page exists. */
    void everyImageExists();

    /*! Verifies that anchors are made from headings as GitHub makes them. */
    void slugMatchesGitHub();

    /*! Verifies how links are classified. */
    void classifiesLinks();

    /*! Verifies that the languages have the same pages and that an unknown one falls back to English. */
    void languages();

private:
    /*! Returns the language folders of the manual in the source tree. */
    static QStringList languagesOnDisk();
};

QStringList HelpManualTest::languagesOnDisk()
{
    QStringList languages;
    for (const QString &name : QDir(QStringLiteral(REQDECK_MANUAL_DIR)).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (QFileInfo::exists(QStringLiteral(REQDECK_MANUAL_DIR "/") + name + QStringLiteral("/manual.json")))
            languages.append(name);
    }
    return languages;
}

void HelpManualTest::listsPagesInOrder()
{
    const HelpManual manual(QStringLiteral(REQDECK_MANUAL_DIR));
    const QVariantList pages = manual.pages();

    QCOMPARE(pages.size(), 12);
    QCOMPARE(pages.first().toMap().value(QStringLiteral("id")), QStringLiteral("Home"));
    QCOMPARE(pages.at(1).toMap().value(QStringLiteral("id")), QStringLiteral("Installation"));
    QCOMPARE(pages.at(1).toMap().value(QStringLiteral("title")), QStringLiteral("Installation"));
    QCOMPARE(manual.language(), QStringLiteral("en"));
    QCOMPARE(manual.baseUrl(), QUrl::fromLocalFile(QStringLiteral(REQDECK_MANUAL_DIR "/en/")));
}

void HelpManualTest::everyPageLoads()
{
    HelpManual manual(QStringLiteral(REQDECK_MANUAL_DIR));
    for (const QString &language : languagesOnDisk()) {
        manual.setLanguage(language);
        for (const QString &id : manual.pageIds()) {
            const QString text = manual.markdown(id);
            QVERIFY2(text.startsWith(u'#'), qPrintable(language + u' ' + id));
            QVERIFY2(text.size() < 200 * 1024, qPrintable(language + u' ' + id));
        }
    }
    QVERIFY(manual.markdown(QStringLiteral("No-Such-Page")).isEmpty());
}

void HelpManualTest::everyLinkResolves()
{
    constexpr const char *pattern = R"re(\]\(([^)\s]+)\))re";
    static const QRegularExpression link(QString::fromLatin1(pattern));
    HelpManual manual(QStringLiteral(REQDECK_MANUAL_DIR));
    QStringList broken;
    for (const QString &language : languagesOnDisk()) {
        manual.setLanguage(language);
        for (const QString &id : manual.pageIds()) {
            for (const QRegularExpressionMatch &match : link.globalMatch(manual.markdown(id))) {
                const QString target = match.captured(1);
                if (target.startsWith(QStringLiteral("images/")))
                    continue;
                const QVariantMap resolved = manual.resolve(target, id);
                const QString kind = resolved.value(QStringLiteral("kind")).toString();
                const QString anchor = resolved.value(QStringLiteral("anchor")).toString();
                if (kind == QStringLiteral("invalid")
                    || (kind == QStringLiteral("page") && !anchor.isEmpty()
                        && manual.anchorPosition(resolved.value(QStringLiteral("page")).toString(), anchor) < 0)) {
                    broken.append(language + u' ' + id + QStringLiteral(" -> ") + target);
                }
            }
        }
    }
    QVERIFY2(broken.isEmpty(), qPrintable(broken.join(u'\n')));
}

void HelpManualTest::everyImageExists()
{
    constexpr const char *pattern = R"re(\]\((images/[^)\s]+)\))re";
    static const QRegularExpression image(QString::fromLatin1(pattern));
    HelpManual manual(QStringLiteral(REQDECK_MANUAL_DIR));
    QStringList missing;
    for (const QString &language : languagesOnDisk()) {
        manual.setLanguage(language);
        for (const QString &id : manual.pageIds()) {
            for (const QRegularExpressionMatch &match : image.globalMatch(manual.markdown(id))) {
                const QString path = QStringLiteral(REQDECK_MANUAL_DIR "/") + language + u'/' + match.captured(1);
                if (!QFileInfo::exists(path))
                    missing.append(language + u' ' + id + QStringLiteral(" -> ") + match.captured(1));
            }
        }
    }
    QVERIFY2(missing.isEmpty(), qPrintable(missing.join(u'\n')));
}

void HelpManualTest::slugMatchesGitHub()
{
    QCOMPARE(HelpManual::slug(QStringLiteral("Secret variables")), QStringLiteral("secret-variables"));
    QCOMPARE(HelpManual::slug(QStringLiteral("OAuth 2.0")), QStringLiteral("oauth-20"));
    QCOMPARE(HelpManual::slug(QStringLiteral("Timeout, redirects, proxy")), QStringLiteral("timeout-redirects-proxy"));
    QCOMPARE(HelpManual::slug(QStringLiteral("1. Open the samples")), QStringLiteral("1-open-the-samples"));
    QCOMPARE(HelpManual::slug(QStringLiteral("API-Schlüssel")), QStringLiteral("api-schlüssel"));
    QCOMPARE(HelpManual::slug(QStringLiteral("Chaining requests with `response()`")),
             QStringLiteral("chaining-requests-with-response"));
}

void HelpManualTest::classifiesLinks()
{
    const HelpManual manual(QStringLiteral(REQDECK_MANUAL_DIR));

    const QVariantMap page = manual.resolve(QStringLiteral("Authentication#oauth-20"), QStringLiteral("Home"));
    QCOMPARE(page.value(QStringLiteral("kind")), QStringLiteral("page"));
    QCOMPARE(page.value(QStringLiteral("page")), QStringLiteral("Authentication"));
    QCOMPARE(page.value(QStringLiteral("anchor")), QStringLiteral("oauth-20"));
    QVERIFY(manual.anchorPosition(QStringLiteral("Authentication"), QStringLiteral("oauth-20")) > 0);

    const QVariantMap here = manual.resolve(QStringLiteral("#promises"), QStringLiteral("Home"));
    QCOMPARE(here.value(QStringLiteral("page")), QStringLiteral("Home"));

    const QVariantMap web = manual.resolve(QStringLiteral("https://github.com/SEB103/ReqDeck/issues"), QStringLiteral("Home"));
    QCOMPARE(web.value(QStringLiteral("kind")), QStringLiteral("external"));
    QCOMPARE(web.value(QStringLiteral("url")), QUrl(QStringLiteral("https://github.com/SEB103/ReqDeck/issues")));

    QCOMPARE(manual.resolve(QStringLiteral("No-Such-Page"), QStringLiteral("Home")).value(QStringLiteral("kind")),
             QStringLiteral("invalid"));
    QCOMPARE(manual.resolve(QStringLiteral("file:///C:/Windows/win.ini"), QStringLiteral("Home")).value(QStringLiteral("kind")),
             QStringLiteral("invalid"));
    QCOMPARE(manual.anchorPosition(QStringLiteral("Home"), QStringLiteral("no-such-heading")), -1);
}

void HelpManualTest::languages()
{
    HelpManual manual(QStringLiteral(REQDECK_MANUAL_DIR));
    const QStringList english = manual.pageIds();
    QSignalSpy changed(&manual, &HelpManual::languageChanged);

    for (const QString &language : languagesOnDisk()) {
        manual.setLanguage(language);
        QCOMPARE(manual.pageIds(), english);
    }
    manual.setLanguage(QStringLiteral("en"));
    changed.clear();
    manual.setLanguage(QStringLiteral("xx"));
    QCOMPARE(manual.language(), QStringLiteral("en"));
    QCOMPARE(changed.count(), 0);
    QCOMPARE(HelpManual::languageForCode(QStringLiteral("de_DE")), QStringLiteral("de"));
    QCOMPARE(HelpManual::languageForCode(QStringLiteral("en_GB")), QStringLiteral("en"));
}

QTEST_GUILESS_MAIN(HelpManualTest)

#include "tst_helpmanual.moc"
