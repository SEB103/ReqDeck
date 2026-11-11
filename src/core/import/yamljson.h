// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef YAMLJSON_H
#define YAMLJSON_H

#include <QByteArray>
#include <QJsonDocument>
#include <QString>

#include <optional>

/**
 * Converts a YAML document to JSON so that the JSON importers can read it.
 *
 * Plain scalars become null, booleans, or numbers by the YAML 1.2 core schema;
 * quoted scalars stay strings. Aliases are expanded, with a limit on the total
 * number of values and on nesting, so a small file cannot grow without bound.
 */
class YamlJson
{
public:
    /** Most values one document may expand to. */
    static constexpr int kMaxValues = 1'000'000;

    /** Deepest nesting of mappings and sequences. */
    static constexpr int kMaxDepth = 256;

    /** Returns the first document of \a yaml as JSON, or nothing with \a errorString set. */
    static std::optional<QJsonDocument> toJson(const QByteArray &yaml, QString *errorString = nullptr);
};

#endif // YAMLJSON_H
