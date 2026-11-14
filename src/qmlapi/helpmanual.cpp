// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/helpmanual.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QImage>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>

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
    return headingPosition(&document, anchor);
}

/*!
 * \brief Returns the text position of the heading \a anchor in the shown \a document.
 *
 * Styling moves text into frames and shortens callout markers, so the window
 * looks the heading up in the document it shows, not in a fresh one.
 * \return The position of the heading's first character, or -1 when there is none.
 */
int HelpManual::anchorIn(QQuickTextDocument *document, const QString &anchor) const
{
    return document ? headingPosition(document->textDocument(), anchor) : -1;
}

/*!
 * \brief Returns the text position of the heading \a anchor in \a document, or -1.
 */
int HelpManual::headingPosition(const QTextDocument *document, const QString &anchor)
{
    if (!document || anchor.isEmpty())
        return -1;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0 && slug(block.text()) == anchor)
            return block.position();
    }
    return -1;
}

/*!
 * \brief Styles the page in \a document after it was loaded from Markdown.
 *
 * Qt lays Markdown out plainly. This pass gives it the look of ReqDeck without a
 * web engine: accent headings with room above them, a taller line, callouts as
 * tinted boxes with a bold label, code and inline code on a shaded ground, and
 * tables with borders and a shaded header row. The GitHub wiki shows the same
 * callouts in its own style, so the source stays plain Markdown.
 * \param document The text document of the help window's text area.
 * \param dark Whether the dark theme is active.
 * \param width Width the text has, to which larger images are scaled down.
 */
void HelpManual::decorate(QQuickTextDocument *document, bool dark, qreal width) const
{
    if (document)
        decorateDocument(document->textDocument(), dark, width);
}

/*!
 * \brief Styles \a text as decorate() describes.
 * \param text The document of a page loaded from Markdown.
 * \param dark Whether the dark theme is active.
 * \param width Width the text has, to which larger images are scaled down.
 */
void HelpManual::decorateDocument(QTextDocument *text, bool dark, qreal width) const
{
    if (!text)
        return;
    const QColor accent = dark ? QColor(0xF0, 0x62, 0x92) : QColor(0xC2, 0x18, 0x5B);
    const QColor codeGround = dark ? QColor(0x2B, 0x2B, 0x2B) : QColor(0xF3, 0xF3, 0xF3);
    const QColor rule = dark ? QColor(0x55, 0x55, 0x55) : QColor(0xD0, 0xD0, 0xD0);
    struct Callout { QString marker; QString label; QColor ground; QColor ink; };
    const QList<Callout> callouts {
        {QStringLiteral("[!TIP]"), tr("Tip"), dark ? QColor(0x1E, 0x3A, 0x2A) : QColor(0xE8, 0xF5, 0xE9),
         dark ? QColor(0x81, 0xC7, 0x84) : QColor(0x2E, 0x7D, 0x32)},
        {QStringLiteral("[!WARNING]"), tr("Warning"), dark ? QColor(0x3E, 0x2F, 0x12) : QColor(0xFF, 0xF3, 0xE0),
         dark ? QColor(0xFF, 0xB7, 0x4D) : QColor(0xE6, 0x51, 0x00)},
        {QStringLiteral("[!NOTE]"), tr("Note"), dark ? QColor(0x1F, 0x2E, 0x3D) : QColor(0xE3, 0xF2, 0xFD),
         dark ? QColor(0x64, 0xB5, 0xF6) : QColor(0x15, 0x65, 0xC0)},
    };

    QTextCursor cursor(text);
    cursor.beginEditBlock();
    const Callout *callout = nullptr;
    // Callouts become frames afterwards: Qt Quick paints the ground of a frame,
    // not of a single paragraph. Each entry is the first and last block of one.
    QList<QPair<int, int>> calloutRanges;
    QList<const Callout *> calloutKinds;
    for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        QTextCursor here(block);
        here.select(QTextCursor::BlockUnderCursor);

        const int heading = format.headingLevel();
        if (heading > 0) {
            format.setTopMargin(heading == 1 ? 4 : 18);
            format.setBottomMargin(6);
            QTextCharFormat ink;
            ink.setForeground(accent);
            ink.setFontWeight(QFont::DemiBold);
            QTextCursor whole(block);
            whole.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            whole.mergeCharFormat(ink);
        } else {
            format.setLineHeight(135, QTextBlockFormat::ProportionalHeight);
        }

        if (format.property(QTextFormat::BlockQuoteLevel).toInt() > 0) {
            if (!callout) {
                for (const Callout &candidate : callouts) {
                    if (block.text().startsWith(candidate.marker)) {
                        callout = &candidate;
                        QTextCursor marker(block);
                        marker.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, int(candidate.marker.size()));
                        QTextCharFormat label;
                        label.setFontWeight(QFont::Bold);
                        label.setForeground(candidate.ink);
                        marker.insertText(candidate.label, label);
                        break;
                    }
                }
            }
            if (callout) {
                if (calloutRanges.isEmpty() || calloutKinds.constLast() != callout
                    || calloutRanges.constLast().second != block.previous().position()) {
                    calloutRanges.append({block.position(), block.position()});
                    calloutKinds.append(callout);
                } else {
                    calloutRanges.last().second = block.position();
                }
            }
            format.setLeftMargin(0);
            format.setTopMargin(2);
            format.setBottomMargin(2);
            format.setIndent(0);
            format.setProperty(QTextFormat::BlockQuoteLevel, 0);
        } else {
            callout = nullptr;
        }

        if (format.hasProperty(QTextFormat::BlockCodeFence) || format.nonBreakableLines()) {
            format.setBackground(codeGround);
            format.setLeftMargin(12);
            format.setRightMargin(12);
            format.setLineHeight(120, QTextBlockFormat::ProportionalHeight);
        }
        QTextCursor(block).setBlockFormat(format);

        // Links in the accent colour, images no wider than the page, inline code
        // on a shaded ground as on GitHub.
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            if (fragment.charFormat().isAnchor()) {
                QTextCursor anchor(text);
                anchor.setPosition(fragment.position());
                anchor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                QTextCharFormat ink;
                ink.setForeground(accent);
                anchor.mergeCharFormat(ink);
            }
            if (fragment.charFormat().isImageFormat() && width > 0) {
                QTextImageFormat image = fragment.charFormat().toImageFormat();
                const QImage picture = text->resource(QTextDocument::ImageResource, QUrl(image.name())).value<QImage>();
                const qreal room = width - 48;
                if (!picture.isNull() && picture.width() > room) {
                    image.setWidth(room);
                    image.setHeight(room * picture.height() / picture.width());
                    QTextCursor scaled(text);
                    scaled.setPosition(fragment.position());
                    scaled.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                    scaled.setCharFormat(image);
                }
            }
            if (fragment.charFormat().fontFixedPitch()) {
                QTextCursor code(text);
                code.setPosition(fragment.position());
                code.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                QTextCharFormat ground;
                ground.setBackground(codeGround);
                code.mergeCharFormat(ground);
            }
        }
    }

    // Callouts: each run of quoted blocks moves into a frame with a tinted ground.
    for (qsizetype i = calloutRanges.size() - 1; i >= 0; --i) {
        QTextCursor range(text);
        range.setPosition(calloutRanges.at(i).first);
        range.setPosition(text->findBlock(calloutRanges.at(i).second).position()
                              + text->findBlock(calloutRanges.at(i).second).length() - 1,
                          QTextCursor::KeepAnchor);
        QTextFrameFormat box;
        box.setBackground(calloutKinds.at(i)->ground);
        box.setPadding(10);
        box.setTopMargin(6);
        box.setBottomMargin(6);
        box.setBorder(0);
        box.setLeftMargin(0);
        range.insertFrame(box);
    }

    // Tables: thin borders, room in the cells, and a shaded header row.
    for (QTextFrame *frame : text->rootFrame()->childFrames()) {
        auto *table = qobject_cast<QTextTable *>(frame);
        if (!table)
            continue;
        QTextTableFormat format = table->format();
        format.setBorder(1);
        format.setBorderBrush(rule);
        format.setBorderCollapse(true);
        format.setCellPadding(6);
        format.setCellSpacing(0);
        format.setWidth(QTextLength(QTextLength::PercentageLength, 100));
        table->setFormat(format);
        for (int column = 0; column < table->columns(); ++column) {
            QTextTableCell cell = table->cellAt(0, column);
            QTextTableCellFormat head = cell.format().toTableCellFormat();
            head.setBackground(dark ? QColor(0x3A, 0x3A, 0x3A) : QColor(0xEE, 0xEE, 0xEE));
            head.setFontWeight(QFont::DemiBold);
            cell.setFormat(head);
        }
    }
    cursor.endEditBlock();
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
