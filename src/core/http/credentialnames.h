// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CREDENTIALNAMES_H
#define CREDENTIALNAMES_H

#include <QStringView>

/**
 * Returns whether \a name, a header, parameter, or field name, marks its value
 * as a credential, for example "X-Api-Key", "access_token", or "key".
 */
bool isCredentialName(QStringView name);

#endif // CREDENTIALNAMES_H
