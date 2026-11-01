// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CURLCONVERTER_H
#define CURLCONVERTER_H

#include <QString>
#include <QStringList>

#include "core/http/requestspec.h"

/**
 * Converts between cURL command lines (bash syntax) and RequestSpec.
 *
 * Parsing then exporting then parsing again yields an equal RequestSpec.
 */
class CurlConverter
{
public:
    /** Outcome of parse(). */
    struct ParseResult
    {
        /** Whether the command could be converted. */
        bool ok {false};
        /** Human-readable reason the command was rejected; empty on success. */
        QString errorString;
        /** Converted request; only meaningful when ok is true. */
        RequestSpec request;
        /** Options that were recognized but have no RequestSpec equivalent. */
        QStringList ignoredOptions;
    };

    /**
     * Parses a cURL \a command copied from a shell or browser (bash quoting,
     * backslash-newline continuations, LF or CRLF line endings).
     */
    static ParseResult parse(const QString &command);

    /**
     * Returns a multi-line bash cURL command for \a spec. Variables are
     * exported unresolved; disabled rows are omitted.
     */
    static QString toCurl(const RequestSpec &spec);
};

#endif // CURLCONVERTER_H
