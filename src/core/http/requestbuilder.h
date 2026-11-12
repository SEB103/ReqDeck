// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REQUESTBUILDER_H
#define REQUESTBUILDER_H

#include <QByteArray>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>

#include "core/http/bodysource.h"
#include "core/http/requestspec.h"
#include "core/http/responsedata.h"
#include "core/http/variableresolver.h"

/** Network request produced from a RequestSpec, ready for HttpExecutor. */
struct BuiltRequest
{
    /** Whether the request could be built; when false only errorString is meaningful. */
    bool ok {false};
    /** Human-readable reason the request could not be built. */
    QString errorString;
    /** Request with URL, headers, and timeout applied; Qt's own redirect handling is off. */
    QNetworkRequest request;
    /** Upper-case request verb. */
    QByteArray verb;
    /** Encoded request body; empty when the request has none or streams it from segments. */
    QByteArray body;
    /** Pieces of a body streamed from files, in order; empty for a body held in body. */
    QList<BodySegment> segments;
    /** Referenced variables that had no value, in order of appearance. */
    QStringList unresolvedVariables;
    /** Whether HttpExecutor follows redirects for this request. */
    bool followRedirects {true};
    /** Headers carrying credentials; removed when a redirect leaves the origin. */
    QList<QByteArray> credentialHeaders;
    /** Name of a query parameter carrying an API key; empty for none. */
    QString credentialQueryName;
    /** A failed check whose server and errors this request accepts; empty for none. Never stored. */
    TlsCheck tlsException;
};

/** Turns a RequestSpec plus environment variables into a BuiltRequest. */
class RequestBuilder
{
public:
    /** Application-wide request defaults. */
    struct Options
    {
        /** User-Agent sent when the request does not set one; empty leaves Qt's default. */
        QString userAgent;
        /** Transfer timeout used when RequestSpec::timeoutMs is 0. */
        int defaultTimeoutMs {30000};
        /** Whether redirects are followed (never from https to http). */
        bool followRedirects {true};
        /** Folder relative file paths resolve against, normally the workspace's; empty for the working directory. */
        QString baseDirectory;
    };

    /**
     * Builds \a spec with \a variables and \a options. Fails for an empty or
     * invalid URL, an unresolved variable in the URL, a non-HTTP(S) scheme,
     * or an invalid header name or value; unresolved variables elsewhere are
     * only reported.
     */
    static BuiltRequest build(const RequestSpec &spec, const VariableResolver &variables,
                              const Options &options);
};

#endif // REQUESTBUILDER_H
