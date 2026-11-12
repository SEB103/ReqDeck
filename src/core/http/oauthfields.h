// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OAUTHFIELDS_H
#define OAUTHFIELDS_H

#include <QString>
#include <QVariant>
#include <QVariantMap>

#include "core/http/requestspec.h"

/** Returns \a settings as a map for QML: "grant" as an int, the other fields as text. */
QVariantMap oauthMap(const OAuth2Settings &settings);

/** Returns \a settings with the field \a name set to \a value; an unknown name or grant leaves it. */
OAuth2Settings withOAuthField(OAuth2Settings settings, const QString &name, const QVariant &value);

#endif // OAUTHFIELDS_H
