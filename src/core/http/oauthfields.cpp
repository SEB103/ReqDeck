// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/oauthfields.h"

/*!
 * \brief Returns \a settings as a map for QML: grant as an int, the rest as text.
 */
QVariantMap oauthMap(const OAuth2Settings &settings)
{
    return {{QStringLiteral("grant"), int(settings.grant)},     {QStringLiteral("authUrl"), settings.authUrl},
            {QStringLiteral("tokenUrl"), settings.tokenUrl},     {QStringLiteral("clientId"), settings.clientId},
            {QStringLiteral("clientSecret"), settings.clientSecret}, {QStringLiteral("scope"), settings.scope},
            {QStringLiteral("audience"), settings.audience}};
}

/*!
 * \brief Returns \a settings with the field \a name set to \a value; unknown names leave it.
 */
OAuth2Settings withOAuthField(OAuth2Settings settings, const QString &name, const QVariant &value)
{
    if (name == QLatin1StringView("grant")) {
        const int grant = value.toInt();
        if (grant == int(OAuthGrant::AuthorizationCode) || grant == int(OAuthGrant::ClientCredentials))
            settings.grant = OAuthGrant(grant);
        return settings;
    }
    const QString text = value.toString();
    if (name == QLatin1StringView("authUrl"))
        settings.authUrl = text;
    else if (name == QLatin1StringView("tokenUrl"))
        settings.tokenUrl = text;
    else if (name == QLatin1StringView("clientId"))
        settings.clientId = text;
    else if (name == QLatin1StringView("clientSecret"))
        settings.clientSecret = text;
    else if (name == QLatin1StringView("scope"))
        settings.scope = text;
    else if (name == QLatin1StringView("audience"))
        settings.audience = text;
    return settings;
}
