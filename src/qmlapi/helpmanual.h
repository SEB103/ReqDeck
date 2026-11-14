// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HELPMANUAL_H
#define HELPMANUAL_H

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class QQuickTextDocument;
class QTextDocument;

/**
 * The user manual shown in the help window, exposed to QML as \c cppManual.
 *
 * The pages are Markdown files, one folder per language with a manual.json that
 * lists them in order. The same files are published to the GitHub wiki, so links
 * between pages are wiki style: "Page" or "Page#section".
 */
class HelpManual : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HelpManual)

    /** Language of the pages, "en" or "de". */
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

    /** Pages in order, as {id, title} rows. */
    Q_PROPERTY(QVariantList pages READ pages NOTIFY languageChanged)

    /** Folder of the pages as a URL, for the images of a page. */
    Q_PROPERTY(QUrl baseUrl READ baseUrl NOTIFY languageChanged)

public:
    /** Reads the manual below \a root, a resource or file folder holding one folder per language. */
    explicit HelpManual(const QString &root = QStringLiteral(":/manual"), QObject *parent = nullptr);

    /** Returns the language of the pages. */
    QString language() const;

    /** Shows the pages in \a language; a language without a manual is ignored. */
    void setLanguage(const QString &language);

    /** Returns the pages in order as {id, title} rows. */
    QVariantList pages() const;

    /** Returns the ids of the pages in order. */
    QStringList pageIds() const;

    /** Returns the folder of the pages as a URL ending in a slash. */
    QUrl baseUrl() const;

    /** Returns the Markdown of the page \a pageId, or an empty string for an unknown page. */
    Q_INVOKABLE QString markdown(const QString &pageId) const;

    /**
     * Says where the link \a link of the page \a currentPage leads, as {kind, page, anchor, url}:
     * kind "page" for a page of the manual, "external" for a web address, "invalid" otherwise.
     */
    Q_INVOKABLE QVariantMap resolve(const QString &link, const QString &currentPage) const;

    /** Returns the text position of the heading \a anchor in the page \a pageId, or -1. */
    Q_INVOKABLE int anchorPosition(const QString &pageId, const QString &anchor) const;

    /**
     * Styles the page in \a document after it was loaded from Markdown: headings in the
     * accent colour, callouts ("> [!TIP]", "> [!WARNING]", "> [!NOTE]") as coloured
     * boxes, code on a shaded ground, tables with a shaded header. \a dark picks the
     * colours of the dark theme; images are scaled down to \a width.
     */
    Q_INVOKABLE void decorate(QQuickTextDocument *document, bool dark, qreal width) const;

    /** Styles \a document as decorate() does; the part that needs no Qt Quick. */
    void decorateDocument(QTextDocument *document, bool dark, qreal width) const;

    /** Returns the text position of the heading \a anchor in the shown \a document, or -1. */
    Q_INVOKABLE int anchorIn(QQuickTextDocument *document, const QString &anchor) const;

    /** Returns the text position of the heading \a anchor in \a document, or -1. */
    static int headingPosition(const QTextDocument *document, const QString &anchor);

    /** Returns the anchor GitHub gives a heading with the text \a heading. */
    static QString slug(const QString &heading);

    /** Returns the manual language for the interface language \a uiCode: "de" for German, else "en". */
    static QString languageForCode(const QString &uiCode);

signals:
    /** Emitted after the pages changed to another language. */
    void languageChanged();

private:
    /** Reads the manifest of \a language; returns false when it has none. */
    bool load(const QString &language);

    /** Folder holding one folder per language. */
    QString m_root;

    /** Language of the pages. */
    QString m_language;

    /** Pages in order as {id, title} rows. */
    QVariantList m_pages;

    /** Page ids in order. */
    QStringList m_ids;
};

#endif // HELPMANUAL_H
