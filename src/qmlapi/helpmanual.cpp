// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/helpmanual.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextBlock>
#include <QTextDocument>

/*!
 * \class HelpManual
 * \brief The Markdown user manual of the help window.
 *
 * Pages are read when shown; the manifest of a language, manual.json, gives
 * their order and titles. English is always present and is the fallback.
 */

/*!
 * \brief Reads the English manual below \a root.
 * \param root Resource or file folder with one folder per language.
 * \param parent The QObject parent.
 */
HelpManual::HelpManual(const QString &root, QObject *parent)
    : QObject(parent)
    , m_root(root)
{
    load(QStringLiteral("en"));
}

/*!
 * \brief Returns the language of the pages.
 */
QString HelpManual::language() const
{
    return m_language;
}

/*!
 * \brief Shows the pages in \a language.
 *
 * A language without a manifest, or the current one, is ignored, so the help
 * stays readable in English.
 * \param language "en" or "de".
 */
void HelpManual::setLanguage(const QString &language)
{
    if (language == m_language || !load(language))
        return;
    emit languageChanged();
}

/*!
 * \brief Returns the pages in order as {id, title} rows.
 */
QVariantList HelpManual::pages() const
{
    return m_pages;
}

/*!
 * \brief Returns the ids of the pages in order.
 */
QStringList HelpManual::pageIds() const
{
    return m_ids;
}

/*!
 * \brief Returns the folder of the pages as a URL ending in a slash.
 *
 * The help window gives it to the text as its base URL, so that the relative
 * images/ paths of a page find their files.
 */
QUrl HelpManual::baseUrl() const
{
    const QString folder = m_root + u'/' + m_language + u'/';
    if (folder.startsWith(u':'))
        return QUrl(QStringLiteral("qrc") + folder);
    return QUrl::fromLocalFile(folder);
}

/*!
 * \brief Returns the Markdown of the page \a pageId.
 * \param pageId A page id from pageIds().
 * \return The text, or an empty string for an unknown page or an unreadable file.
 */
QString HelpManual::markdown(const QString &pageId) const
{
    if (!m_ids.contains(pageId))
        return {};
    QFile file(m_root + u'/' + m_language + u'/' + pageId + QStringLiteral(".md"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

/*!
 * \brief Says where the link \a link of the page \a currentPage leads.
 *
 * Links between pages are wiki style, "Page" or "Page#section", as the GitHub
 * wiki needs them; "#section" stays on \a currentPage. http and https
 * addresses are external and open in the system browser. Anything else, such
 * as a file: URL or an unknown page, is invalid and is not followed.
 * \return A map with kind ("page", "external", or "invalid"), page, anchor, and url.
 */
QVariantMap HelpManual::resolve(const QString &link, const QString &currentPage) const
{
    QVariantMap result {{QStringLiteral("kind"), QStringLiteral("invalid")}};
    const QUrl url(link);
    if (url.scheme() == QLatin1StringView("http") || url.scheme() == QLatin1StringView("https")) {
        result.insert(QStringLiteral("kind"), QStringLiteral("external"));
        result.insert(QStringLiteral("url"), url);
        return result;
    }
    if (!url.scheme().isEmpty())
        return result;
    const QString page = link.section(u'#', 0, 0);
    const QString target = page.isEmpty() ? currentPage : page;
    if (!m_ids.contains(target))
        return result;
    result.insert(QStringLiteral("kind"), QStringLiteral("page"));
    result.insert(QStringLiteral("page"), target);
    result.insert(QStringLiteral("anchor"), link.section(u'#', 1));
    return result;
}

/*!
 * \brief Returns the text position of the heading \a anchor in the page \a pageId.
 *
 * The page is laid out as the help window lays it out, from Markdown in the
 * GitHub dialect, so the position matches the text the window shows.
 * \return The position of the heading's first character, or -1 when there is none.
 */
int HelpManual::anchorPosition(const QString &pageId, const QString &anchor) const
{
    const QString text = markdown(pageId);
    if (text.isEmpty() || anchor.isEmpty())
        return -1;
    QTextDocument document;
    document.setMarkdown(text);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0 && slug(block.text()) == anchor)
            return block.position();
    }
    return -1;
}

/*!
 * \brief Returns the anchor GitHub gives a heading with the text \a heading.
 *
 * Lower case; letters, digits, hyphens, and underscores stay, spaces become
 * hyphens, everything else is dropped; "OAuth 2.0" becomes "oauth-20".
 */
QString HelpManual::slug(const QString &heading)
{
    QString slug;
    for (const QChar c : heading.trimmed().toLower()) {
        if (c.isLetterOrNumber() || c == u'-' || c == u'_')
            slug += c;
        else if (c == u' ')
            slug += u'-';
    }
    return slug;
}

/*!
 * \brief Returns the manual language for the interface language \a uiCode.
 * \param uiCode A locale code such as "de_DE".
 * \return "de" for German, else "en".
 */
QString HelpManual::languageForCode(const QString &uiCode)
{
    return uiCode.startsWith(QLatin1StringView("de")) ? QStringLiteral("de") : QStringLiteral("en");
}

/*!
 * \internal
 * \brief Reads the manifest of \a language.
 * \return Whether it exists and lists pages; the state is unchanged otherwise.
 */
bool HelpManual::load(const QString &language)
{
    QFile file(m_root + u'/' + language + QStringLiteral("/manual.json"));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonArray rows = QJsonDocument::fromJson(file.readAll()).object().value(QLatin1StringView("pages")).toArray();
    if (rows.isEmpty())
        return false;
    m_language = language;
    m_pages.clear();
    m_ids.clear();
    for (const QJsonValue &row : rows) {
        const QString id = row.toObject().value(QLatin1StringView("id")).toString();
        m_ids.append(id);
        m_pages.append(QVariantMap {{QStringLiteral("id"), id},
                                    {QStringLiteral("title"), row.toObject().value(QLatin1StringView("title")).toString()}});
    }
    return true;
}
