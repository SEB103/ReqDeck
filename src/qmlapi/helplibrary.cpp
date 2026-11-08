// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/helplibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include <algorithm>

/*!
 * \brief Loads help.<language>.json from \a directory.
 *
 * Help is an aid, so a missing or broken file is logged and leaves no topics
 * instead of stopping the application; the panel then shows nothing.
 * \param language "en" or "ru".
 * \param directory Folder of the help files, the resource folder by default.
 * \param parent The QObject parent.
 */
HelpLibrary::HelpLibrary(const QString &language, const QString &directory, QObject *parent)
    : QObject(parent)
    , m_language(language)
{
    QFile file(QDir(directory).filePath(QStringLiteral("help.%1.json").arg(language)));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << QStringLiteral("Help file %1 could not be read.").arg(file.fileName());
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        qWarning().noquote() << QStringLiteral("Help file %1 is not valid.").arg(file.fileName());
        return;
    }

    m_labels = document.object().value(QLatin1StringView("labels")).toObject().toVariantMap();
    m_links = document.object().value(QLatin1StringView("links")).toObject().toVariantMap();

    QHash<QString, QString> chapterTitles;
    const QJsonArray chapters = document.object().value(QLatin1StringView("chapters")).toArray();
    for (const QJsonValue &value : chapters) {
        const QJsonObject chapter = value.toObject();
        const QString id = chapter.value(QLatin1StringView("id")).toString();
        m_chapters.append(id);
        chapterTitles.insert(id, chapter.value(QLatin1StringView("title")).toString());
    }

    // The panel names the chapter it points to, because a PDF opened by the
    // system viewer starts at its first page.
    const QJsonObject topics = document.object().value(QLatin1StringView("topics")).toObject();
    for (auto it = topics.constBegin(); it != topics.constEnd(); ++it) {
        QVariantMap topic = it.value().toObject().toVariantMap();
        const QString chapter = topic.value(QStringLiteral("chapter")).toString();
        if (chapterTitles.contains(chapter))
            topic.insert(QStringLiteral("chapterTitle"), chapterTitles.value(chapter));
        m_topics.insert(it.key(), topic);
    }
}

/*!
 * \brief Returns the language of the topics.
 */
QString HelpLibrary::language() const
{
    return m_language;
}

/*!
 * \brief Returns the topic \a id.
 * \param id The topic id, for example "request.url".
 * \return The parts of the topic, with the title of its chapter as
 * "chapterTitle", or an empty map for an unknown id.
 */
QVariantMap HelpLibrary::topic(const QString &id) const
{
    return m_topics.value(id);
}

/*!
 * \brief Returns whether a topic \a id exists.
 * \param id The topic id.
 */
bool HelpLibrary::hasTopic(const QString &id) const
{
    return m_topics.contains(id);
}

/*!
 * \brief Returns the headings of the help panel.
 *
 * They are in the language of the topics, which can differ from the language of
 * the application interface.
 */
QVariantMap HelpLibrary::labels() const
{
    return m_labels;
}

/*!
 * \brief Returns the ids of all topics, sorted.
 */
QStringList HelpLibrary::topicIds() const
{
    QStringList ids = m_topics.keys();
    std::sort(ids.begin(), ids.end());
    return ids;
}

/*!
 * \brief Returns the ids of the chapters of the guide, in order.
 */
QStringList HelpLibrary::chapterIds() const
{
    return m_chapters;
}

/*!
 * \brief Returns the web addresses offered in the Help menu.
 *
 * Keys are "practice", a public API to try requests on, and "statusCodes", a
 * reference of HTTP status codes in the language of the topics.
 */
QVariantMap HelpLibrary::links() const
{
    return m_links;
}

/*!
 * \brief Returns where the PDF guide of this language is installed.
 *
 * The guide is a file in the help folder next to the executable, not a
 * resource, so that the system PDF viewer can open it.
 */
QString HelpLibrary::guidePath() const
{
    const QString directory = m_guideDirectory.isEmpty()
        ? QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("help"))
        : m_guideDirectory;
    return QDir(directory).filePath(QStringLiteral("ReqDeck-Guide.%1.pdf").arg(m_language));
}

/*!
 * \brief Returns guidePath() as a file URL, safe for paths with spaces.
 */
QUrl HelpLibrary::guideUrl() const
{
    return QUrl::fromLocalFile(guidePath());
}

/*!
 * \brief Returns whether the PDF guide of this language exists.
 */
bool HelpLibrary::hasGuide() const
{
    return QFileInfo(guidePath()).isFile();
}

/*!
 * \brief Looks for the guide in \a directory instead of the help folder next to
 * the executable.
 * \param directory The folder of the guide; empty restores the default.
 */
void HelpLibrary::setGuideDirectory(const QString &directory)
{
    m_guideDirectory = directory;
}

/*!
 * \brief Returns the help language for \a locale.
 * \param locale Usually the system locale.
 * \return "ru" for Russian, else "en".
 */
QString HelpLibrary::languageFor(const QLocale &locale)
{
    return locale.language() == QLocale::Russian ? QStringLiteral("ru") : QStringLiteral("en");
}
