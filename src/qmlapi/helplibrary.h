// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HELPLIBRARY_H
#define HELPLIBRARY_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

/**
 * The help topics shown by F1, exposed to QML as \c cppHelp.
 *
 * One JSON file per language holds a short topic for every control; each topic
 * names the section of the user manual its "More" opens. The language follows
 * the interface; English is the fallback.
 */
class HelpLibrary : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HelpLibrary)

    /** Language of the topics, "en" or "de". */
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)

public:
    /** Loads help.<language>.json from \a directory, or the English file when that one is missing. */
    explicit HelpLibrary(const QString &language, const QString &directory = QStringLiteral(":/help"),
                         QObject *parent = nullptr);

    /** Returns the language of the topics. */
    QString language() const;

    /** Shows the topics in \a language; a language without a help file falls back to English. */
    void setLanguage(const QString &language);

    /** Returns the topic \a id with title, what, why, example, next, and more; empty when unknown. */
    Q_INVOKABLE QVariantMap topic(const QString &id) const;

    /** Returns whether a topic \a id exists. */
    Q_INVOKABLE bool hasTopic(const QString &id) const;

    /** Returns the headings of the help panel: why, example, next, more, chapter, close. */
    Q_INVOKABLE QVariantMap labels() const;

    /** Returns the ids of all topics, sorted. */
    QStringList topicIds() const;

    /** Returns the web addresses of the Help menu: practice and statusCodes. */
    Q_INVOKABLE QVariantMap links() const;

signals:
    /** Emitted after the topics were loaded in another language. */
    void languageChanged();

private:
    /** Reads help.<language>.json; returns false when the file is missing or broken. */
    bool load(const QString &language);

    /** Folder of the help files. */
    QString m_directory;

    /** Language of the topics. */
    QString m_language;

    /** Topics by id. */
    QHash<QString, QVariantMap> m_topics;

    /** Headings of the help panel. */
    QVariantMap m_labels;

    /** Web addresses of the Help menu. */
    QVariantMap m_links;
};

#endif // HELPLIBRARY_H
