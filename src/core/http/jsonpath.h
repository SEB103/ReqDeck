// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef JSONPATH_H
#define JSONPATH_H

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

/** The outcome of a JSONPath query. */
struct JsonPathResult
{
    /** Whether the path could be read and evaluated. */
    bool ok {false};
    /** Why not, for the user. */
    QString errorString;
    /** The values the path selects: array elements in order, object members sorted by name, as Qt keeps them. */
    QJsonArray matches;
};

/**
 * Evaluates a subset of JSONPath (RFC 9535) without any script.
 *
 * Supported: the root $, child names .name and ['name'], the wildcard * and
 * [*], indexes [n] (negative from the end), slices [start:end:step] with a
 * positive step, descendants ..name, ..* and ..[...], unions [a,b], and
 * filters [?(@.a.b op literal)] or [?(@.a)] with ==, !=, <, <=, >, >= and
 * number, string, true, false, or null literals.
 */
class JsonPath
{
public:
    /** Most values a query may visit before it stops. */
    static constexpr int kMaxVisited = 1000000;

    /** Returns the values \a path selects in \a root. */
    static JsonPathResult evaluate(const QJsonValue &root, const QString &path);
};

#endif // JSONPATH_H
