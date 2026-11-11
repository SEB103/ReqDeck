// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef REQUESTJSON_H
#define REQUESTJSON_H

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <optional>

#include "core/http/requestspec.h"

/** Returns the JSON form of \a row; "enabled" is written only when false. */
QJsonObject keyValueToJson(const KeyValue &row);

/** Parses one key/value row; nullopt when \a value is not a valid row. */
std::optional<KeyValue> keyValueFromJson(const QJsonValue &value);

/** Returns the JSON form of \a auth; only non-default members are written. */
QJsonObject authToJson(const RequestAuth &auth);

/** Parses authentication settings; nullopt with a message in \a errorString when a member is invalid. */
std::optional<RequestAuth> authFromJson(const QJsonObject &object, QString *errorString = nullptr);

/** Returns the JSON form of \a request; only non-default members are written. */
QJsonObject requestToJson(const RequestSpec &request);

/**
 * Parses a request. Absent members take their defaults and unknown members are
 * ignored; a member of the wrong type or with an unknown value fails with a
 * message in \a errorString when it is not null.
 */
std::optional<RequestSpec> requestFromJson(const QJsonObject &object,
                                           QString *errorString = nullptr);

#endif // REQUESTJSON_H
