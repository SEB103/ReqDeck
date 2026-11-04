// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/workspaceserializer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <algorithm>

#include "core/workspace/requestjson.h"

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

/*!
 * \internal
 * \brief Returns the JSON form of the tree rooted at \a node.
 */
QJsonObject nodeToJson(const CollectionNode &node)
{
    QJsonObject object;
    object.insert(QLatin1StringView("id"), node.id);
    object.insert(QLatin1StringView("name"), node.name);
    if (node.kind == CollectionNode::Kind::Request) {
        object.insert(QLatin1StringView("kind"), QLatin1StringView("request"));
        object.insert(QLatin1StringView("request"), requestToJson(node.request));
        return object;
    }

    object.insert(QLatin1StringView("kind"), QLatin1StringView("folder"));
    if (!node.children.isEmpty()) {
        QJsonArray children;
        for (const CollectionNode &child : node.children)
            children.append(nodeToJson(child));
        object.insert(QLatin1StringView("children"), children);
    }
    return object;
}

/*!
 * \internal
 * \brief Returns the JSON form of \a environment.
 *
 * The value of a secret variable is never written: it belongs to SecretStore,
 * and a workspace file must be safe to share.
 */
QJsonObject environmentToJson(const Environment &environment)
{
    QJsonArray variables;
    for (const EnvironmentVariable &variable : environment.variables) {
        QJsonObject row;
        row.insert(QLatin1StringView("key"), variable.key);
        if (!variable.secret)
            row.insert(QLatin1StringView("value"), variable.value);
        if (!variable.enabled)
            row.insert(QLatin1StringView("enabled"), false);
        if (variable.secret)
            row.insert(QLatin1StringView("secret"), true);
        variables.append(row);
    }

    QJsonObject object;
    object.insert(QLatin1StringView("id"), environment.id);
    object.insert(QLatin1StringView("name"), environment.name);
    object.insert(QLatin1StringView("variables"), variables);
    return object;
}

/*!
 * \internal
 * \brief Validates the id member \a value and records it in \a seenIds.
 * \return An empty string on success, otherwise the error message.
 */
QString claimId(const QJsonValue &value, QSet<QString> &seenIds)
{
    const QString id = value.toString();
    if (id.isEmpty())
        return WorkspaceSerializer::errorToString(WorkspaceSerializer::Error::InvalidSchema);
    if (seenIds.contains(id))
        return QStringLiteral("Duplicate item id: %1").arg(id);
    seenIds.insert(id);
    return {};
}

/*!
 * \internal
 * \brief Parses the collection items in \a items, recursing into folders.
 *
 * \param items The JSON array of items.
 * \param seenIds Ids seen so far in the whole collection.
 * \param nodes Receives the parsed nodes.
 * \return An empty string on success, otherwise the error message.
 */
QString nodesFromJson(const QJsonArray &items, QSet<QString> &seenIds, QList<CollectionNode> &nodes)
{
    const QString invalid =
        WorkspaceSerializer::errorToString(WorkspaceSerializer::Error::InvalidSchema);

    for (const QJsonValue &item : items) {
        if (!item.isObject())
            return invalid;
        const QJsonObject object = item.toObject();

        if (const QString error = claimId(object.value(QLatin1StringView("id")), seenIds);
            !error.isEmpty()) {
            return error;
        }
        if (!hasOptionalType(object, QLatin1StringView("name"), QJsonValue::String))
            return invalid;

        CollectionNode node;
        node.id = object.value(QLatin1StringView("id")).toString();
        node.name = object.value(QLatin1StringView("name")).toString();

        const QString kind = object.value(QLatin1StringView("kind")).toString();
        if (kind == QLatin1StringView("request")) {
            const QJsonValue request = object.value(QLatin1StringView("request"));
            if (!request.isObject())
                return invalid;
            QString requestError;
            std::optional<RequestSpec> spec = requestFromJson(request.toObject(), &requestError);
            if (!spec)
                return QStringLiteral("Item %1: %2").arg(node.id, requestError);
            node.kind = CollectionNode::Kind::Request;
            node.request = std::move(*spec);
        } else if (kind == QLatin1StringView("folder")) {
            if (!hasOptionalType(object, QLatin1StringView("children"), QJsonValue::Array))
                return invalid;
            const QJsonArray children = object.value(QLatin1StringView("children")).toArray();
            if (const QString error = nodesFromJson(children, seenIds, node.children);
                !error.isEmpty()) {
                return error;
            }
        } else {
            return invalid;
        }
        nodes.append(std::move(node));
    }
    return {};
}

/*!
 * \internal
 * \brief Parses the environments in \a items.
 *
 * \param items The JSON array of environments.
 * \param environments Receives the parsed environments.
 * \return An empty string on success, otherwise the error message.
 */
QString environmentsFromJson(const QJsonArray &items, QList<Environment> &environments)
{
    const QString invalid =
        WorkspaceSerializer::errorToString(WorkspaceSerializer::Error::InvalidSchema);

    QSet<QString> seenIds;
    for (const QJsonValue &item : items) {
        if (!item.isObject())
            return invalid;
        const QJsonObject object = item.toObject();

        if (const QString error = claimId(object.value(QLatin1StringView("id")), seenIds);
            !error.isEmpty()) {
            return error;
        }
        if (!hasOptionalType(object, QLatin1StringView("name"), QJsonValue::String)
            || !hasOptionalType(object, QLatin1StringView("variables"), QJsonValue::Array)) {
            return invalid;
        }

        Environment environment;
        environment.id = object.value(QLatin1StringView("id")).toString();
        environment.name = object.value(QLatin1StringView("name")).toString();

        for (const QJsonValue &entry : object.value(QLatin1StringView("variables")).toArray()) {
            if (!entry.isObject())
                return invalid;
            const QJsonObject row = entry.toObject();
            if (!hasOptionalType(row, QLatin1StringView("key"), QJsonValue::String)
                || !hasOptionalType(row, QLatin1StringView("value"), QJsonValue::String)
                || !hasOptionalType(row, QLatin1StringView("enabled"), QJsonValue::Bool)
                || !hasOptionalType(row, QLatin1StringView("secret"), QJsonValue::Bool)) {
                return invalid;
            }
            environment.variables.append({row.value(QLatin1StringView("key")).toString(),
                                          row.value(QLatin1StringView("value")).toString(),
                                          row.value(QLatin1StringView("enabled")).toBool(true),
                                          row.value(QLatin1StringView("secret")).toBool(false)});
        }
        environments.append(std::move(environment));
    }
    return {};
}

} // namespace

/*!
 * \brief Serializes \a data into a JSON object.
 */
QJsonObject WorkspaceSerializer::toJson(const WorkspaceData &data)
{
    QJsonArray collection;
    for (const CollectionNode &node : data.collection)
        collection.append(nodeToJson(node));

    QJsonArray environments;
    for (const Environment &environment : data.environments)
        environments.append(environmentToJson(environment));

    QJsonObject root;
    root.insert(kSchemaKey, data.schemaVersion);
    if (!data.id.isEmpty())
        root.insert(QLatin1StringView("id"), data.id);
    root.insert(QLatin1StringView("displayName"), data.displayName);
    root.insert(QLatin1StringView("collection"), collection);
    root.insert(QLatin1StringView("environments"), environments);
    root.insert(QLatin1StringView("activeEnvironmentId"), data.activeEnvironmentId);
    root.insert(QLatin1StringView("settings"), data.settings);
    return root;
}

/*!
 * \brief Loads and validates the workspace file at \a filePath.
 *
 * The file must exist and contain a JSON object with an integer \c schema
 * member in the supported range. The optional members \c id, \c displayName,
 * \c collection, \c environments, \c activeEnvironmentId, and \c settings fall
 * back to empty values when absent, but must have the expected JSON type when
 * present.
 *
 * The workspace id must be a UUID, because SecretStore uses it as a file name
 * and a shared file must not be able to point outside the secrets directory.
 * Collection items and environments are validated completely, including unique
 * ids. An active environment id naming no environment loads as empty, since
 * that state is harmless and easily caused by editing the file by hand.
 *
 * On any failure the returned LoadResult has \c ok set to false and describes
 * the reason; the caller decides how to surface it.
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

    if (!hasOptionalType(root, QLatin1StringView("id"), QJsonValue::String)
        || !hasOptionalType(root, QLatin1StringView("displayName"), QJsonValue::String)
        || !hasOptionalType(root, QLatin1StringView("collection"), QJsonValue::Array)
        || !hasOptionalType(root, QLatin1StringView("environments"), QJsonValue::Array)
        || !hasOptionalType(root, QLatin1StringView("activeEnvironmentId"), QJsonValue::String)
        || !hasOptionalType(root, QLatin1StringView("settings"), QJsonValue::Object)) {
        return failure(Error::InvalidSchema);
    }

    LoadResult result;
    result.data.schemaVersion = schemaVersion;
    result.data.id = root.value(QLatin1StringView("id")).toString();
    if (!result.data.id.isEmpty() && QUuid::fromString(result.data.id).isNull())
        return failure(Error::InvalidSchema, QStringLiteral("Invalid workspace id."));

    result.data.displayName = root.value(QLatin1StringView("displayName")).toString();

    QSet<QString> seenIds;
    if (const QString error = nodesFromJson(root.value(QLatin1StringView("collection")).toArray(),
                                            seenIds, result.data.collection);
        !error.isEmpty()) {
        return failure(Error::InvalidSchema, error);
    }
    if (const QString error =
            environmentsFromJson(root.value(QLatin1StringView("environments")).toArray(),
                                 result.data.environments);
        !error.isEmpty()) {
        return failure(Error::InvalidSchema, error);
    }

    const QString activeId = root.value(QLatin1StringView("activeEnvironmentId")).toString();
    const bool activeExists = std::any_of(
        result.data.environments.cbegin(), result.data.environments.cend(),
        [&activeId](const Environment &environment) { return environment.id == activeId; });
    if (activeExists)
        result.data.activeEnvironmentId = activeId;

    result.data.settings = root.value(QLatin1StringView("settings")).toObject();
    result.ok = true;
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
