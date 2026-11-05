// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RESPONSEVIEW_H
#define RESPONSEVIEW_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "core/http/responsedata.h"
#include "framework/bodyformatter.h"
#include "qmlapi/keyvaluemodel.h"

class BodyHighlighter;

QT_BEGIN_NAMESPACE
class QQuickTextDocument;
QT_END_NAMESPACE

/**
 * The last response, prepared for the response pane; exposed to QML as
 * \c cppResponse. \c bodyKind uses the numeric values of BodyKind.
 */
class ResponseView : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ResponseView)

    /** Whether a result is available, also when it is an error. */
    Q_PROPERTY(bool hasResponse READ hasResponse NOTIFY responseChanged)

    /** Whether a request is running. */
    Q_PROPERTY(bool busy READ busy NOTIFY responseChanged)

    /** Name of the request this response belongs to. */
    Q_PROPERTY(QString requestName READ requestName NOTIFY responseChanged)

    /** HTTP status code; 0 when no response was received. */
    Q_PROPERTY(int statusCode READ statusCode NOTIFY responseChanged)

    /** HTTP reason phrase. */
    Q_PROPERTY(QString reasonPhrase READ reasonPhrase NOTIFY responseChanged)

    /** First digit of the status code; 0 when there is none. */
    Q_PROPERTY(int statusClass READ statusClass NOTIFY responseChanged)

    /** Failure text; empty when the request worked. */
    Q_PROPERTY(QString errorText READ errorText NOTIFY responseChanged)

    /** Warning about the request that was sent; empty when there is none. */
    Q_PROPERTY(QString warningText READ warningText NOTIFY responseChanged)

    /** Duration of the request in milliseconds. */
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY responseChanged)

    /** Body size in bytes. */
    Q_PROPERTY(qint64 sizeBytes READ sizeBytes NOTIFY responseChanged)

    /** Body size in readable units, for example "1.5 KB". */
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY responseChanged)

    /** Value of the Content-Type header. */
    Q_PROPERTY(QString contentType READ contentType NOTIFY responseChanged)

    /** What the body contains: 0 text, 1 JSON, 2 XML, 3 binary. */
    Q_PROPERTY(int bodyKind READ bodyKind NOTIFY responseChanged)

    /** Text for the body view, Pretty or Raw. */
    Q_PROPERTY(QString bodyText READ bodyText NOTIFY responseChanged)

    /** Whether bodyText shows only the start of the body. */
    Q_PROPERTY(bool truncated READ truncated NOTIFY responseChanged)

    /** Whether the body can be formatted. */
    Q_PROPERTY(bool canPretty READ canPretty NOTIFY responseChanged)

    /** Whether formatted text is preferred; stays set across responses. */
    Q_PROPERTY(bool pretty READ pretty WRITE setPretty NOTIFY responseChanged)

    /** Whether the body colors suit a dark background. */
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)

    /** Response headers. */
    Q_PROPERTY(KeyValueModel *headersModel READ headersModel CONSTANT)

public:
    /** Creates an empty view. */
    explicit ResponseView(QObject *parent = nullptr);

    /** Returns whether a result is available. */
    bool hasResponse() const;

    /** Returns whether a request is running. */
    bool busy() const;

    /** Returns the name of the request. */
    QString requestName() const;

    /** Returns the HTTP status code. */
    int statusCode() const;

    /** Returns the HTTP reason phrase. */
    QString reasonPhrase() const;

    /** Returns the first digit of the status code. */
    int statusClass() const;

    /** Returns the failure text. */
    QString errorText() const;

    /** Returns the warning text. */
    QString warningText() const;

    /** Returns the duration in milliseconds. */
    qint64 elapsedMs() const;

    /** Returns the body size in bytes. */
    qint64 sizeBytes() const;

    /** Returns the body size in readable units. */
    QString sizeText() const;

    /** Returns the Content-Type value. */
    QString contentType() const;

    /** Returns what the body contains. */
    int bodyKind() const;

    /** Returns the text for the body view. */
    QString bodyText() const;

    /** Returns whether bodyText() shows only the start of the body. */
    bool truncated() const;

    /** Returns whether the body can be formatted. */
    bool canPretty() const;

    /** Returns whether formatted text is preferred. */
    bool pretty() const;

    /** Sets whether formatted text is preferred. */
    void setPretty(bool pretty);

    /** Returns whether the body colors suit a dark background. */
    bool darkTheme() const;

    /** Sets whether the body colors suit a dark background. */
    void setDarkTheme(bool dark);

    /** Returns the response headers. */
    KeyValueModel *headersModel() const;

    /** Colors the text of \a document, the document of the body view. */
    Q_INVOKABLE void attachHighlighter(QQuickTextDocument *document);

    /** Shows that the request \a requestName is running. */
    void setBusy(const QString &requestName);

    /** Shows \a response; \a unresolvedVariables become a warning. */
    void setResponse(const ResponseData &response, const QStringList &unresolvedVariables);

    /** Shows that the request could not be sent because of \a message. */
    void setBuildError(const QString &message, const QStringList &unresolvedVariables);

    /** Removes everything shown. */
    void clear();

signals:
    /** Emitted when anything shown changes. */
    void responseChanged();

    /** Emitted when darkTheme() changes. */
    void darkThemeChanged();

private:
    /** Forgets the shown result, keeping requestName() and pretty(). */
    void resetResult();

    /** Returns the formatted body, building it on first use. */
    const QString &formattedBody() const;

    /** Response headers; owned through the QObject tree. */
    KeyValueModel *m_headers {nullptr};

    /** Colors the body view; owned through the QObject tree. */
    BodyHighlighter *m_highlighter {nullptr};

    /** The response shown; the body is released after decoding. */
    ResponseData m_response;

    /** Decoded body. */
    DecodedBody m_body;

    /** Formatted body; built on first use because large bodies take time. */
    mutable QString m_formatted;

    /** Whether m_formatted is valid. */
    mutable bool m_hasFormatted {false};

    /** Name of the request. */
    QString m_requestName;

    /** Failure text. */
    QString m_errorText;

    /** Warning text. */
    QString m_warningText;

    /** Whether a result is available. */
    bool m_hasResponse {false};

    /** Whether a request is running. */
    bool m_busy {false};

    /** Whether formatted text is preferred. */
    bool m_pretty {true};

    /** Whether the body colors suit a dark background. */
    bool m_darkTheme {false};
};

#endif // RESPONSEVIEW_H
