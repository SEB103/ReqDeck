// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/secretstore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <utility>

namespace {
/*!
 * \internal
 * \brief Schema version of a secrets file.
 */
constexpr int kSecretsSchemaVersion = 1;

/*!
 * \internal
 * \brief Sets \a errorString to \a message when it is not null and returns false.
 */
bool fail(QString *errorString, const QString &message)
{
    if (errorString)
        *errorString = message;
    return false;
}

/*!
 * \internal
 * \brief Returns whether \a id is a UUID and therefore safe as a file name.
 */
bool isValidWorkspaceId(const QString &id)
{
    return !QUuid::fromString(id).isNull();
}

/*!
 * \internal
 * \brief Moves the malformed file at \a path aside, replacing an older copy.
 *
 * Keeping the file gives the user a chance to recover the values by hand, and
 * the next save must not overwrite it.
 */
void setAside(const QString &path)
{
    const QString target = path + QStringLiteral(".corrupt");
    QFile::remove(target);
    QFile::rename(path, target);
}
} // namespace

/*!
 * \brief Creates a store over \a directory.
 * \param directory The directory holding the secrets files.
 */
SecretStore::SecretStore(QString directory)
    : m_directory(std::move(directory))
{
}

/*!
 * \brief Returns the path of the secrets file of \a workspaceId.
 * \param workspaceId The UUID of the workspace.
 */
QString SecretStore::filePath(const QString &workspaceId) const
{
    return QDir(m_directory).filePath(workspaceId + QStringLiteral(".json"));
}

/*!
 * \brief Fills the secret variables of \a data from the store.
 *
 * Only variables that still have the secret flag and a stored value change, so
 * a variable the user made public keeps its value from the workspace file. A
 * workspace without a valid id has nothing stored.
 * \param data The workspace to complete.
 * \param errorString Receives the reason on failure when it is not null.
 * \return False when the file is unreadable or malformed; \a data is then unchanged.
 */
bool SecretStore::applyTo(WorkspaceData &data, QString *errorString) const
{
    if (!isValidWorkspaceId(data.id))
        return true;

    const QString path = filePath(data.id);
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return fail(errorString, QStringLiteral("The secrets file could not be read."));

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    const QJsonObject root = document.object();
    const QJsonValue environments = root.value(QLatin1StringView("environments"));
    if (!document.isObject() || root.value(QLatin1StringView("schema")).toInt() != kSecretsSchemaVersion
        || !environments.isObject()) {
        setAside(path);
        return fail(errorString, QStringLiteral("The secrets file is not valid."));
    }

    const QJsonObject stored = environments.toObject();
    for (Environment &environment : data.environments) {
        const QJsonObject values = stored.value(environment.id).toObject();
        for (EnvironmentVariable &variable : environment.variables) {
            const QJsonValue value = values.value(variable.key);
            if (variable.secret && value.isString())
                variable.value = value.toString();
        }
    }
    return true;
}

/*!
 * \brief Replaces the stored values of \a data.
 *
 * Every secret variable with a non-empty value is written, including disabled
 * ones. With no value left the file is removed, so a workspace without secrets
 * leaves nothing behind. Writing goes through QSaveFile: a failure keeps the
 * previous file.
 * \param data The workspace to store.
 * \param errorString Receives the reason on failure when it is not null.
 * \return True on success.
 */
bool SecretStore::save(const WorkspaceData &data, QString *errorString) const
{
    const QString writeError = QStringLiteral("The secret values could not be written.");
    if (!isValidWorkspaceId(data.id))
        return fail(errorString, writeError);

    QJsonObject environments;
    for (const Environment &environment : data.environments) {
        QJsonObject values;
        for (const EnvironmentVariable &variable : environment.variables) {
            if (variable.secret && !variable.value.isEmpty())
                values.insert(variable.key, variable.value);
        }
        if (!values.isEmpty())
            environments.insert(environment.id, values);
    }

    const QString path = filePath(data.id);
    if (environments.isEmpty()) {
        if (QFile::exists(path) && !QFile::remove(path))
            return fail(errorString, writeError);
        return true;
    }

    if (!QDir().mkpath(m_directory))
        return fail(errorString, writeError);

    QJsonObject root;
    root.insert(QLatin1StringView("schema"), kSecretsSchemaVersion);
    root.insert(QLatin1StringView("environments"), environments);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(errorString, writeError);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return fail(errorString, writeError);
    return true;
}
