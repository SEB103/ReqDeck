// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qmlapi/responseview.h"

#include <QQuickTextDocument>

#include "qmlapi/bodyhighlighter.h"

namespace {
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
 * \brief Returns whether bodyText() shows only the start of the body.
 */
bool ResponseView::truncated() const
{
    if (m_body.kind == BodyKind::Binary)
        return false;
    const QString &text = m_pretty && canPretty() ? formattedBody() : m_body.text;
    return text.size() > kMaxDisplayChars;
}

/*!
 * \brief Returns whether the body can be formatted.
 *
 * Text beyond the display limit is not formatted: nobody could see the result,
 * and formatting would only make it longer.
 */
bool ResponseView::canPretty() const
{
    return (m_body.kind == BodyKind::Json || m_body.kind == BodyKind::Xml)
           && m_body.text.size() <= kMaxDisplayChars;
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
    m_highlighter->setDark(dark);
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
 */
void ResponseView::setBusy(const QString &requestName)
{
    resetResult();
    m_requestName = requestName;
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
    m_response = response;
    m_response.body.clear();
    m_warningText = unresolvedWarning(unresolvedVariables);

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
    emit responseChanged();
}

/*!
 * \internal
 * \brief Forgets the shown result, keeping requestName() and pretty().
 */
void ResponseView::resetResult()
{
    m_response = ResponseData();
    m_body = DecodedBody();
    m_formatted.clear();
    m_hasFormatted = false;
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
