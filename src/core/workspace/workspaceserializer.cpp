// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/workspaceserializer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>

/*!
    \class WorkspaceSerializer
    \brief Reads and writes WorkspaceData to and from .reqdeck JSON files.

    \internal
*/

namespace {

/*!
 * \internal
 * \brief JSON key holding the schema version.
 */
constexpr QLatin1StringView kSchemaKey("schema");

/*!
 * \internal
 * \brief Returns whether the optional member \a key of \a root is absent or of \a type.
 *
 * Optional members may be missing (their defaults apply), but a member that is
 * present with the wrong JSON type means the file does not describe a workspace.
 */
bool hasOptionalType(const QJsonObject &root, QLatin1StringView key, QJsonValue::Type type)
{
    const QJsonValue value = root.value(key);
    return value.isUndefined() || value.type() == type;
}

/*!
 * \internal
 * \brief Builds a failed LoadResult for \a error, using \a detail when it is not empty.
 */
WorkspaceSerializer::LoadResult failure(WorkspaceSerializer::Error error,
                                        const QString &detail = QString())
{
    WorkspaceSerializer::LoadResult result;
    result.error = error;
    result.errorString = detail.isEmpty() ? WorkspaceSerializer::errorToString(error) : detail;
    return result;
}

} // namespace

/*!
 * \brief Serializes \a data into a JSON object.
 */
QJsonObject WorkspaceSerializer::toJson(const WorkspaceData &data)
{
    QJsonObject root;
    root.insert(kSchemaKey, data.schemaVersion);
    root.insert(QLatin1StringView("displayName"), data.displayName);
    root.insert(QLatin1StringView("collection"), data.collection);
    root.insert(QLatin1StringView("environments"), data.environments);
    root.insert(QLatin1StringView("activeEnvironmentId"), data.activeEnvironmentId);
    root.insert(QLatin1StringView("settings"), data.settings);
    return root;
}

/*!
 * \brief Loads and validates the workspace file at \a filePath.
 *
 * The file must exist and contain a JSON object with an integer \c schema
 * member in the supported range. The optional members \c displayName,
 * \c collection, \c environments, \c activeEnvironmentId, and \c settings fall
 * back to empty values when absent, but must have the expected JSON type when
 * present. On any failure the returned LoadResult has \c ok set to false and
 * describes the reason; the caller decides how to surface it.
 */
WorkspaceSerializer::LoadResult WorkspaceSerializer::load(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile())
        return failure(Error::FileNotFound);

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return failure(Error::ReadFailed);

    const QByteArray content = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(content, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return failure(Error::InvalidJson, parseError.errorString());
    if (!document.isObject())
        return failure(Error::InvalidJson);

    const QJsonObject root = document.object();
    const QJsonValue schema = root.value(kSchemaKey);
    if (!schema.isDouble() || schema.toDouble() != double(schema.toInt()))
        return failure(Error::InvalidSchema);

    const int schemaVersion = schema.toInt();
    if (schemaVersion < 1 || schemaVersion > kWorkspaceSchemaVersion)
        return failure(Error::UnsupportedVersion);

    if (!hasOptionalType(root, QLatin1StringView("displayName"), QJsonValue::String)
        || !hasOptionalType(root, QLatin1StringView("collection"), QJsonValue::Array)
        || !hasOptionalType(root, QLatin1StringView("environments"), QJsonValue::Array)
        || !hasOptionalType(root, QLatin1StringView("activeEnvironmentId"), QJsonValue::String)
        || !hasOptionalType(root, QLatin1StringView("settings"), QJsonValue::Object)) {
        return failure(Error::InvalidSchema);
    }

    LoadResult result;
    result.ok = true;
    result.data.schemaVersion = schemaVersion;
    result.data.displayName = root.value(QLatin1StringView("displayName")).toString();
    result.data.collection = root.value(QLatin1StringView("collection")).toArray();
    result.data.environments = root.value(QLatin1StringView("environments")).toArray();
    result.data.activeEnvironmentId =
        root.value(QLatin1StringView("activeEnvironmentId")).toString();
    result.data.settings = root.value(QLatin1StringView("settings")).toObject();
    return result;
}

/*!
 * \brief Writes \a data as pretty-printed JSON to \a filePath.
 *
 * Parent directories are created when missing. A QSaveFile is used so a failed
 * write does not leave a partially written workspace file behind. Returns true
 * on success; on failure returns false and sets \a errorString when it is not
 * null.
 */
bool WorkspaceSerializer::save(const QString &filePath, const WorkspaceData &data,
                               QString *errorString)
{
    const auto fail = [errorString]() {
        if (errorString)
            *errorString = errorToString(Error::WriteFailed);
        return false;
    };

    const QDir directory = QFileInfo(filePath).absoluteDir();
    if (!directory.exists() && !QDir().mkpath(directory.absolutePath()))
        return fail();

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return fail();

    file.write(QJsonDocument(toJson(data)).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return fail();

    return true;
}

/*!
 * \brief Returns a human-readable description of \a error.
 */
QString WorkspaceSerializer::errorToString(Error error)
{
    switch (error) {
    case Error::None:
        return QString();
    case Error::FileNotFound:
        return QStringLiteral("The workspace file does not exist.");
    case Error::ReadFailed:
        return QStringLiteral("The workspace file could not be opened for reading.");
    case Error::InvalidJson:
        return QStringLiteral("The workspace file does not contain valid JSON.");
    case Error::InvalidSchema:
        return QStringLiteral("The file is not a valid ReqDeck workspace.");
    case Error::UnsupportedVersion:
        return QStringLiteral("The workspace file uses an unsupported schema version.");
    case Error::WriteFailed:
        return QStringLiteral("The workspace file could not be written.");
    }
    return QString();
}
