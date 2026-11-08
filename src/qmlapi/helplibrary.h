// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HELPLIBRARY_H
#define HELPLIBRARY_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

class QLocale;

/**
 * The help topics shown by F1, in one language, exposed to QML as \c cppHelp.
 *
 * Topics and the chapters of the guide come from one JSON file per language, so
 * the panel and the PDF guide cannot drift apart.
 */
class HelpLibrary : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HelpLibrary)

    /** Language of the topics, "en" or "ru". */
    Q_PROPERTY(QString language READ language CONSTANT)

public:
    /** Loads help.<language>.json from \a directory; a missing or broken file leaves no topics. */
    explicit HelpLibrary(const QString &language, const QString &directory = QStringLiteral(":/help"),
                         QObject *parent = nullptr);

    /** Returns the language of the topics. */
    QString language() const;

    /** Returns the topic \a id with title, what, why, example, next, chapter, and chapterTitle; empty when unknown. */
    Q_INVOKABLE QVariantMap topic(const QString &id) const;

    /** Returns whether a topic \a id exists. */
    Q_INVOKABLE bool hasTopic(const QString &id) const;

    /** Returns the headings of the help panel: why, example, next, more, close. */
    Q_INVOKABLE QVariantMap labels() const;

    /** Returns the ids of all topics, sorted. */
    QStringList topicIds() const;

    /** Returns the ids of the chapters of the guide, in order. */
    QStringList chapterIds() const;

    /** Returns the web addresses of the Help menu: practice and statusCodes. */
    Q_INVOKABLE QVariantMap links() const;

    /** Returns where the PDF guide of this language is installed. */
    Q_INVOKABLE QString guidePath() const;

    /** Returns guidePath() as a file URL. */
    Q_INVOKABLE QUrl guideUrl() const;

    /** Returns whether the PDF guide of this language exists. */
    Q_INVOKABLE bool hasGuide() const;

    /** Looks for the guide in \a directory; empty means the help folder next to the executable. */
    void setGuideDirectory(const QString &directory);

    /** Returns "ru" for a Russian \a locale and "en" for any other. */
    static QString languageFor(const QLocale &locale);

private:
    /** Language of the topics. */
    QString m_language;

    /** Topics by id. */
    QHash<QString, QVariantMap> m_topics;

    /** Headings of the help panel. */
    QVariantMap m_labels;

    /** Chapter ids in order. */
    QStringList m_chapters;

    /** Web addresses of the Help menu. */
    QVariantMap m_links;

    /** Folder of the guide, empty for the help folder next to the executable. */
    QString m_guideDirectory;
};

#endif // HELPLIBRARY_H
