// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HTTPENCODING_H
#define HTTPENCODING_H

#include <QByteArray>
#include <QList>

#include "core/http/requestspec.h"

/**
 * Encodes enabled pairs with a non-empty key as "key=value" joined by '&'.
 * Keys and values are percent-encoded (unreserved characters stay literal, a
 * space becomes %20); used for query strings, form bodies, and cURL export.
 */
QByteArray encodeKeyValuePairs(const QList<KeyValue> &pairs);

/**
 * Returns the Content-Type sent for a body in \a mode with \a rawType when the
 * user sets none, for example "application/json"; empty for BodyMode::None.
 */
QByteArray defaultContentType(BodyMode mode, RawBodyType rawType);

#endif // HTTPENCODING_H
