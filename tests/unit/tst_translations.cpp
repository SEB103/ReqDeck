// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTranslator>
#include <QXmlStreamReader>
#include <QtTest>

namespace {

/*!
 * \internal
 * \brief One message of a Qt Linguist catalog.
 */
struct Message
{
    /*! Context, usually the QML file or the C++ class. */
    QString context;
    /*! English source text. */
    QString source;
    /*! Translation, empty when missing. */
    QString translation;
    /*! The type attribute of the translation: empty, "unfinished", "vanished", or "obsolete". */
    QString type;
};

/*!
 * \internal
 * \brief Reads every message of the catalog \a path.
 */
QList<Message> readCatalog(const QString &path)
{
    QList<Message> messages;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return messages;
    QXmlStreamReader xml(&file);
    QString context;
    Message message;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (xml.name() == QLatin1StringView("name")) {
            context = xml.readElementText();
        } else if (xml.name() == QLatin1StringView("message")) {
            message = Message {context, {}, {}, {}};
        } else if (xml.name() == QLatin1StringView("source")) {
            message.source = xml.readElementText();
        } else if (xml.name() == QLatin1StringView("translation")) {
            message.type = xml.attributes().value(QLatin1StringView("type")).toString();
            message.translation = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            messages.append(message);
        }
    }
    return messages;
}

/*!
 * \internal
 * \brief Returns the placeholders %1 to %99, also as %L1, that \a text uses.
 */
QSet<QString> placeholders(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("%L?([1-9][0-9]?)"));
    QSet<QString> found;
    for (const QRegularExpressionMatch &match : pattern.globalMatch(text))
        found.insert(match.captured(1));
    return found;
}

/*!
 * \internal
 * \brief Returns how many single & marks a keyboard accelerator in \a text; && is a literal &.
 */
int accelerators(QString text)
{
    text.remove(QStringLiteral("&&"));
    // Entities such as &amp; in rich text are no accelerators.
    static const QRegularExpression entity(QStringLiteral("&[a-z]+;"));
    text.remove(entity);
    return int(text.count(u'&'));
}

} // namespace

/*! Verifies that the interface can be translated and that the German catalog is complete. */
class TranslationsTest : public QObject
{
    Q_OBJECT

private slots:
    /*! Verifies that no visible QML text bypasses qsTr(). */
    void noLiteralUiText();

    /*! Verifies that every message of the German catalog has a translation in use. */
    void everyMessageTranslated();

    /*! Verifies that a translation keeps the placeholders of its source. */
    void placeholdersMatch();

    /*! Verifies that a translation keeps the keyboard accelerator of its source. */
    void acceleratorsMatch();

    /*! Verifies that the compiled German catalog loads and translates the menus. */
    void catalogLoads();
};

void TranslationsTest::noLiteralUiText()
{
    // A visible property bound to nothing but a string literal.
    constexpr const char *pattern =
        R"re(^\s*(text|title|placeholderText|ToolTip\.text|Accessible\.name|Accessible\.description)\s*:\s*"([^"]*)"\s*$)re";
    static const QRegularExpression property(QString::fromLatin1(pattern));
    static const QRegularExpression letters(QStringLiteral("[A-Za-z]{2}"));
    QStringList findings;
    QDirIterator it(QStringLiteral(REQDECK_QML_DIR), {QStringLiteral("*.qml")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QStringList lines = QString::fromUtf8(file.readAll()).split(u'\n');
        for (int i = 0; i < lines.size(); ++i) {
            const QRegularExpressionMatch match = property.match(lines.at(i).trimmed().prepend(u' '));
            if (match.hasMatch() && match.captured(2).contains(letters))
                findings.append(QStringLiteral("%1:%2: %3").arg(path).arg(i + 1).arg(lines.at(i).trimmed()));
        }
    }
    QVERIFY2(findings.isEmpty(), qPrintable(findings.join(u'\n')));
}

void TranslationsTest::everyMessageTranslated()
{
    const QList<Message> messages = readCatalog(QStringLiteral(REQDECK_TS_DIR "/ReqDeck_de_DE.ts"));
    QVERIFY(messages.size() > 400);
    QStringList missing;
    for (const Message &message : messages) {
        if (!message.type.isEmpty() || message.translation.isEmpty())
            missing.append(message.context + u": " + message.source + u" [" + message.type + u']');
    }
    QVERIFY2(missing.isEmpty(), qPrintable(QString::number(missing.size()) + u'\n' + missing.mid(0, 20).join(u'\n')));
}

void TranslationsTest::placeholdersMatch()
{
    QStringList wrong;
    for (const Message &message : readCatalog(QStringLiteral(REQDECK_TS_DIR "/ReqDeck_de_DE.ts"))) {
        if (!message.translation.isEmpty() && placeholders(message.source) != placeholders(message.translation))
            wrong.append(message.source + u" -> " + message.translation);
    }
    QVERIFY2(wrong.isEmpty(), qPrintable(wrong.join(u'\n')));
}

void TranslationsTest::acceleratorsMatch()
{
    QStringList wrong;
    for (const Message &message : readCatalog(QStringLiteral(REQDECK_TS_DIR "/ReqDeck_de_DE.ts"))) {
        if (!message.translation.isEmpty() && accelerators(message.source) != accelerators(message.translation))
            wrong.append(message.source + u" -> " + message.translation);
    }
    QVERIFY2(wrong.isEmpty(), qPrintable(wrong.join(u'\n')));
}

void TranslationsTest::catalogLoads()
{
    QTranslator translator;
    QVERIFY(translator.load(QStringLiteral(":/translations/ReqDeck_de_DE")));
    QCOMPARE(translator.translate("BsMenuBar", "&File"), QStringLiteral("&Datei"));
    QCOMPARE(translator.translate("BsMenuBar", "&Help"), QStringLiteral("&Hilfe"));
    QCOMPARE(translator.translate("BsLanguageButton", "Interface language"), QStringLiteral("Sprache der Oberfläche"));
}

QTEST_GUILESS_MAIN(TranslationsTest)

#include "tst_translations.moc"
