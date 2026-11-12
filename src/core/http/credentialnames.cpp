// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/credentialnames.h"

#include <QString>

#include <algorithm>
#include <array>

namespace {
/*!
 * \internal
 * \brief Parts of header, parameter, and field names that mark a credential.
 */
constexpr std::array<QLatin1StringView, 17> kCredentialNameParts {
    QLatin1StringView("authorization"), QLatin1StringView("authentication"),
    QLatin1StringView("x-auth"),        QLatin1StringView("cookie"),
    QLatin1StringView("token"),         QLatin1StringView("secret"),
    QLatin1StringView("password"),      QLatin1StringView("passwd"),
    QLatin1StringView("pwd"),           QLatin1StringView("apikey"),
    QLatin1StringView("api-key"),       QLatin1StringView("api_key"),
    QLatin1StringView("access_key"),    QLatin1StringView("accesskey"),
    QLatin1StringView("session"),       QLatin1StringView("signature"),
    QLatin1StringView("jwt"),
};

/*!
 * \internal
 * \brief Short names that mark a credential only as a whole name: "key", not "monkey".
 */
constexpr std::array<QLatin1StringView, 4> kCredentialNames {
    QLatin1StringView("key"),
    QLatin1StringView("auth"),
    QLatin1StringView("pass"),
    QLatin1StringView("sig"),
};
} // namespace

/*!
 * \brief Returns whether \a name marks its value as a credential.
 *
 * A name is a credential when it is one of a few short names as a whole, such
 * as "key", or contains a longer part such as "token" or "password", in any
 * case. History and the view of what was sent hide such values.
 * \param name The name to check; surrounding spaces are ignored.
 */
bool isCredentialName(QStringView name)
{
    const QStringView trimmed = name.trimmed();
    return std::any_of(kCredentialNames.cbegin(), kCredentialNames.cend(),
                       [trimmed](QLatin1StringView candidate) {
                           return trimmed.compare(candidate, Qt::CaseInsensitive) == 0;
                       })
        || std::any_of(kCredentialNameParts.cbegin(), kCredentialNameParts.cend(),
                       [trimmed](QLatin1StringView part) {
                           return trimmed.contains(part, Qt::CaseInsensitive);
                       });
}
