// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/helplibrary.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

/*!
 * \brief Loads the topics of \a language from \a directory.
 *
 * Help is an aid, so a missing or broken file is logged and leaves no topics
 * instead of stopping the application. A language without its own file shows
 * the English topics.
 * \param language "en" or "de".
 * \param directory Folder of the help files, the resource folder by default.
 * \param parent The QObject parent.
 */
HelpLibrary::HelpLibrary(const QString &language, const QString &directory, QObject *parent)
    : QObject(parent)
    , m_directory(directory)
{
    if (!load(language) && language != QLatin1StringView("en"))
        load(QStringLiteral("en"));
}

/*!
 * \brief Returns the language of the topics.
 */
QString HelpLibrary::language() const
{
    return m_language;
}

/*!
 * \brief Shows the topics in \a language.
 *
 * A language without a help file falls back to English, so F1 always explains
 * something. languageChanged() is emitted when the language shown changed.
 * \param language "en" or "de".
 */
void HelpLibrary::setLanguage(const QString &language)
{
    const QString previous = m_language;
    if (language == previous)
        return;
    if (!load(language) && !load(QStringLiteral("en")))
        return;
    if (m_language != previous)
        emit languageChanged();
}

/*!
 * \brief Returns the topic \a id with title, what, why, example, next, and more.
 * \return The topic, or an empty map when \a id is unknown.
 */
QVariantMap HelpLibrary::topic(const QString &id) const
{
    return m_topics.value(id);
}

/*!
 * \brief Returns whether a topic \a id exists.
 */
bool HelpLibrary::hasTopic(const QString &id) const
{
    return m_topics.contains(id);
}

/*!
 * \brief Returns the headings of the help panel in the language of the topics.
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
 * \brief Returns the web addresses of the Help menu: practice and statusCodes.
 */
QVariantMap HelpLibrary::links() const
{
    return m_links;
}

/*!
 * \internal
 * \brief Reads help.<language>.json from the help folder.
 * \return Whether the file was read; the topics are unchanged otherwise.
 */
bool HelpLibrary::load(const QString &language)
{
    QFile file(QDir(m_directory).filePath(QStringLiteral("help.%1.json").arg(language)));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        qWarning().noquote() << QStringLiteral("Help file %1 is not valid.").arg(file.fileName());
        return false;
    }
    m_language = language;
    m_labels = document.object().value(QLatin1StringView("labels")).toObject().toVariantMap();
    m_links = document.object().value(QLatin1StringView("links")).toObject().toVariantMap();
    m_topics.clear();
    const QJsonObject topics = document.object().value(QLatin1StringView("topics")).toObject();
    for (auto it = topics.constBegin(); it != topics.constEnd(); ++it)
        m_topics.insert(it.key(), it.value().toObject().toVariantMap());
    return true;
}
