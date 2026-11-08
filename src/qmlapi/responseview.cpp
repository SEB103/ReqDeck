// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/responseview.h"

#include <QDir>
#include <QFile>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSaveFile>

#include "qmlapi/bodyhighlighter.h"

namespace {
/*!
 * \internal
 * \brief Longest run of non-space characters wrapped at word boundaries.
 *
 * Looking for a break opportunity is quadratic in the length of such a run: a
 * text item needs more than a minute for 1 MiB without spaces.
 */
constexpr qsizetype kMaxWordWrappedRun = 1024;

/*!
 * \internal
 * \brief Returns whether \a text has a run longer than kMaxWordWrappedRun.
 */
bool hasLongRun(QStringView text)
{
    qsizetype run = 0;
    for (const QChar c : text) {
        run = c.isSpace() ? 0 : run + 1;
        if (run > kMaxWordWrappedRun)
            return true;
    }
    return false;
}

/*!
 * \internal
 * \brief Returns the warning for \a names; empty when there are none.
 */
QString unresolvedWarning(const QStringList &names)
{
    if (names.isEmpty())
        return {};
    QStringList references;
    for (const QString &name : names)
        references.append(QStringLiteral("{{%1}}").arg(name));
    return QObject::tr("Unresolved variables: %1").arg(references.join(QStringLiteral(", ")));
}
} // namespace

/*!
 * \brief Creates an empty view.
 * \param parent The QObject parent.
 */
ResponseView::ResponseView(QObject *parent)
    : QObject(parent)
    , m_headers(new KeyValueModel(this))
    , m_highlighter(new BodyHighlighter(this))
{
}

bool ResponseView::hasResponse() const
{
    return m_hasResponse;
}

bool ResponseView::busy() const
{
    return m_busy;
}

QString ResponseView::requestId() const
{
    return m_requestId;
}

/*!
 * \brief Returns the name of the request the response belongs to.
 */
QString ResponseView::requestName() const
{
    return m_requestName;
}

int ResponseView::statusCode() const
{
    return m_response.statusCode;
}

QString ResponseView::reasonPhrase() const
{
    return m_response.reasonPhrase;
}

int ResponseView::statusClass() const
{
    return m_response.statusCode / 100;
}

QString ResponseView::errorText() const
{
    return m_errorText;
}

QString ResponseView::warningText() const
{
    return m_warningText;
}

qint64 ResponseView::elapsedMs() const
{
    return m_response.elapsedMs;
}

qint64 ResponseView::sizeBytes() const
{
    return m_response.sizeBytes;
}

/*!
 * \brief Returns the body size in readable units.
 */
QString ResponseView::sizeText() const
{
    const qint64 bytes = m_response.sizeBytes;
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(double(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
}

QString ResponseView::contentType() const
{
    return m_response.contentType;
}

int ResponseView::bodyKind() const
{
    return int(m_body.kind);
}

/*!
 * \brief Returns the text for the body view.
 *
 * The view handles at most kMaxDisplayChars; longer text is cut there and
 * reported by truncated(). Binary data is described instead of shown.
 */
QString ResponseView::bodyText() const
{
    if (m_body.kind == BodyKind::Binary)
        return tr("Binary body, %1. Not shown.").arg(sizeText());
    const QString &text = m_pretty && canPretty() ? formattedBody() : m_body.text;
    return text.left(kMaxDisplayChars);
}

/*!
 * \brief Returns the number of lines of bodyText().
 *
 * The line numbers of the view use this instead of TextEdit.lineCount: a binding
 * to that property makes the text item count all lines on each change, which
 * nearly doubled the time to show a body of many short lines.
 */
int ResponseView::bodyLineCount() const
{
    return int(bodyText().count(u'\n')) + 1;
}

/*!
 * \brief Returns whether bodyText() shows only the start of the body.
 */
bool ResponseView::truncated() const
{
    if (m_body.kind == BodyKind::Binary)
        return false;
    if (bodyOnDisk())
        return true;
    const QString &text = m_pretty && canPretty() ? formattedBody() : m_body.text;
    return text.size() > kMaxDisplayChars;
}

/*!
 * \brief Returns whether the body can be formatted.
 *
 * Text beyond the display limit is not formatted: nobody could see the result,
 * and formatting would only make it longer. Text that formatting leaves as it
 * is, because it is broken or already formatted, offers no choice either, and it
 * must not lose line wrapping for nothing.
 */
bool ResponseView::canPretty() const
{
    // The start of a body is not a document; formatting it would mislead.
    return !bodyOnDisk() && (m_body.kind == BodyKind::Json || m_body.kind == BodyKind::Xml)
           && m_body.text.size() <= kMaxDisplayChars && formattedBody() != m_body.text;
}

/*!
 * \brief Returns whether the body was too large for memory, so that only its
 * start is shown and the whole of it lies in a temporary file.
 */
bool ResponseView::bodyOnDisk() const
{
    return m_response.bodyFile != nullptr;
}

/*!
 * \brief Returns the start positions of \a query in bodyText(), ignoring case.
 *
 * The positions refer to the text shown, so the view can select them directly.
 * The search stops at kMaxFindMatches: a single character in a megabyte matches
 * too often for any count to help.
 * \param query The text to look for.
 * \return Start positions in order; empty for an empty query or no match.
 */
QVariantList ResponseView::findInBody(const QString &query) const
{
    QVariantList matches;
    if (query.isEmpty())
        return matches;
    const QString text = bodyText();
    for (qsizetype from = text.indexOf(query, 0, Qt::CaseInsensitive);
         from >= 0 && matches.size() < kMaxFindMatches;
         from = text.indexOf(query, from + 1, Qt::CaseInsensitive)) {
        matches.append(int(from));
    }
    return matches;
}

bool ResponseView::canSaveBody() const
{
    return m_hasResponse && m_response.sizeBytes > 0;
}

/*!
 * \brief Returns a file name for the body.
 *
 * The last part of the URL path names the file, with characters Windows does
 * not allow replaced; "response" stands in for a URL without a path. A name
 * without an extension gets one from the kind of body.
 */
QString ResponseView::suggestedFileName() const
{
    QString name = QUrl::fromPercentEncoding(m_response.finalUrl.path().section(u'/', -1).toUtf8());
    static const QRegularExpression unsafe(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]"));
    name.replace(unsafe, QStringLiteral("_"));
    name = name.trimmed();
    if (name.isEmpty() || name == u'.' || name == QLatin1StringView(".."))
        name = QStringLiteral("response");
    if (name.contains(u'.'))
        return name;

    switch (m_body.kind) {
    case BodyKind::Json:
        return name + QStringLiteral(".json");
    case BodyKind::Xml:
        return name + QStringLiteral(".xml");
    case BodyKind::Binary:
        return name + QStringLiteral(".bin");
    case BodyKind::Text:
        break;
    }
    return name + (m_response.contentType.contains(QLatin1StringView("html"), Qt::CaseInsensitive)
                       ? QStringLiteral(".html")
                       : QStringLiteral(".txt"));
}

/*!
 * \brief Writes the whole body, as received after decompression, to \a fileUrl.
 *
 * A body on disk is copied in pieces, so saving it needs no memory either. The
 * target is replaced only once everything was written, so a failed save leaves
 * an existing file as it was.
 * \param fileUrl A local file, usually chosen in a file dialog.
 * \return Whether the file was written.
 */
bool ResponseView::saveBody(const QUrl &fileUrl)
{
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QSaveFile out(path);
    QString error;
    if (!out.open(QIODevice::WriteOnly)) {
        error = out.errorString();
    } else if (m_response.bodyFile) {
        QFile in(m_response.bodyFile->fileName());
        if (!in.open(QIODevice::ReadOnly))
            error = in.errorString();
        while (error.isEmpty() && !in.atEnd()) {
            const QByteArray chunk = in.read(1024 * 1024);
            if (out.write(chunk) != chunk.size())
                error = out.errorString();
        }
    } else if (out.write(m_rawBody) != m_rawBody.size()) {
        error = out.errorString();
    }
    if (error.isEmpty() && !out.commit())
        error = out.errorString();

    if (!error.isEmpty()) {
        emit bodySaveFailed(tr("The response could not be saved to %1: %2")
                                .arg(QDir::toNativeSeparators(path), error));
        return false;
    }
    emit bodySaved(QDir::toNativeSeparators(path));
    return true;
}

/*!
 * \brief Returns whether the body has a run too long to wrap at word boundaries.
 */
bool ResponseView::wrapAnywhere() const
{
    return m_wrapAnywhere;
}

/*!
 * \brief Returns whether the request failed only because the server's
 * certificate is not trusted, so that sending it again for this host once,
 * without the check, is a meaningful offer.
 */
bool ResponseView::canTrustCertificate() const
{
    return m_hasResponse && m_response.error == ResponseError::Tls && !m_response.tlsErrors.isEmpty()
           && !m_response.tlsErrorsIgnored && !m_response.finalUrl.host().isEmpty();
}

/*!
 * \brief Returns the host whose certificate failed or was not checked.
 */
QString ResponseView::certificateHost() const
{
    return m_response.finalUrl.host();
}

bool ResponseView::pretty() const
{
    return m_pretty;
}

void ResponseView::setPretty(bool pretty)
{
    if (m_pretty == pretty)
        return;
    m_pretty = pretty;
    emit responseChanged();
}

bool ResponseView::darkTheme() const
{
    return m_darkTheme;
}

void ResponseView::setDarkTheme(bool dark)
{
    if (m_darkTheme == dark)
        return;
    m_darkTheme = dark;
    m_highlighter->selectPalette(dark);
    emit darkThemeChanged();
}

KeyValueModel *ResponseView::headersModel() const
{
    return m_headers;
}

/*!
 * \brief Colors the text of \a document.
 *
 * The view owns the highlighter, so that QML needs no C++ type for it.
 * \param document The document of the body view.
 */
void ResponseView::attachHighlighter(QQuickTextDocument *document)
{
    m_highlighter->setDocument(document ? document->textDocument() : nullptr);
}

/*!
 * \brief Shows that the request \a requestName is running.
 * \param requestName The name of the request.
 * \param requestId The id of the request.
 */
void ResponseView::setBusy(const QString &requestName, const QString &requestId)
{
    resetResult();
    m_requestName = requestName;
    m_requestId = requestId;
    m_busy = true;
    emit responseChanged();
}

/*!
 * \brief Shows \a response.
 *
 * A response can carry a status and an error together, for example when the
 * connection broke during the body; both are then shown.
 * \param response The result of the request.
 * \param unresolvedVariables Variables that had no value when it was built.
 */
void ResponseView::setResponse(const ResponseData &response, const QStringList &unresolvedVariables)
{
    resetResult();
    m_hasResponse = true;
    m_body = decodeBody(response.body, response.contentType);
    m_highlighter->setKind(int(m_body.kind));
    m_wrapAnywhere = hasLongRun(QStringView(m_body.text).first(qMin(m_body.text.size(), kMaxDisplayChars)));
    m_response = response;
    // The bytes as received stay for saving; a body on disk keeps them there.
    if (!response.bodyFile)
        m_rawBody = response.body;
    m_response.body.clear();
    QStringList warnings;
    if (response.tlsErrorsIgnored) {
        warnings.append(tr("The certificate of %1 was not checked: %2")
                            .arg(response.finalUrl.host(), response.tlsErrors.join(QStringLiteral("; "))));
    }
    if (!unresolvedVariables.isEmpty())
        warnings.append(unresolvedWarning(unresolvedVariables));
    m_warningText = warnings.join(u'\n');

    if (response.error != ResponseError::None) {
        QStringList lines {response.errorString};
        lines += response.tlsErrors;
        m_errorText = lines.join(u'\n');
    }

    QList<KeyValue> rows;
    rows.reserve(response.headers.size());
    for (const auto &[name, value] : response.headers)
        rows.append({QString::fromLatin1(name), QString::fromUtf8(value), true});
    m_headers->setRows(rows);

    emit responseChanged();
}

/*!
 * \brief Shows that the request could not be sent.
 * \param message The reason.
 * \param unresolvedVariables Variables that had no value.
 */
void ResponseView::setBuildError(const QString &message, const QStringList &unresolvedVariables)
{
    resetResult();
    m_hasResponse = true;
    m_errorText = message;
    m_warningText = unresolvedWarning(unresolvedVariables);
    emit responseChanged();
}

/*!
 * \brief Removes everything shown.
 */
void ResponseView::clear()
{
    resetResult();
    m_requestName.clear();
    m_requestId.clear();
    emit responseChanged();
}

/*!
 * \internal
 * \brief Forgets the shown result, keeping requestName() and pretty().
 */
void ResponseView::resetResult()
{
    m_response = ResponseData();
    m_rawBody.clear();
    m_body = DecodedBody();
    m_formatted.clear();
    m_hasFormatted = false;
    m_wrapAnywhere = false;
    m_errorText.clear();
    m_warningText.clear();
    m_hasResponse = false;
    m_busy = false;
    m_headers->setRows({});
}

/*!
 * \internal
 * \brief Returns the formatted body, building it on first use.
 */
const QString &ResponseView::formattedBody() const
{
    if (!m_hasFormatted) {
        m_formatted = m_body.kind == BodyKind::Json ? prettyJson(m_body.text) : prettyXml(m_body.text);
        m_hasFormatted = true;
    }
    return m_formatted;
}
