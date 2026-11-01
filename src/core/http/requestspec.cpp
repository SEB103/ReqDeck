// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/requestspec.h"

#include <QLatin1StringView>

#include <array>
#include <utility>

namespace {
/*!
 * \internal
 * \brief Maps every HttpMethod to its upper-case request verb.
 */
constexpr std::array<std::pair<HttpMethod, QLatin1StringView>, 7> kMethodVerbs {{
    {HttpMethod::Get, QLatin1StringView("GET")},
    {HttpMethod::Post, QLatin1StringView("POST")},
    {HttpMethod::Put, QLatin1StringView("PUT")},
    {HttpMethod::Patch, QLatin1StringView("PATCH")},
    {HttpMethod::Delete, QLatin1StringView("DELETE")},
    {HttpMethod::Head, QLatin1StringView("HEAD")},
    {HttpMethod::Options, QLatin1StringView("OPTIONS")},
}};
} // namespace

/*!
 * \brief Returns the upper-case request verb of \a method.
 * \param method The HTTP method.
 */
QByteArray httpMethodVerb(HttpMethod method)
{
    for (const auto &[candidate, verb] : kMethodVerbs) {
        if (candidate == method)
            return QByteArray(verb.data(), verb.size());
    }
    return QByteArrayLiteral("GET");
}

/*!
 * \brief Parses a method name such as "post" or " PATCH ".
 *
 * Surrounding whitespace is ignored and the comparison is case-insensitive.
 * \param text The method name.
 * \return The matching method, or std::nullopt for an unknown or empty name.
 */
std::optional<HttpMethod> httpMethodFromString(QStringView text)
{
    const QStringView trimmed = text.trimmed();
    for (const auto &[method, verb] : kMethodVerbs) {
        if (trimmed.compare(verb, Qt::CaseInsensitive) == 0)
            return method;
    }
    return std::nullopt;
}
