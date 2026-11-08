// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/secretstore.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

#include "core/workspace/jsonfile.h"

namespace {
/*!
 * \internal
 * \brief Schema version of a secrets file this build writes: encrypted values.
 */
constexpr int kSecretsSchemaVersion = 2;

/*!
 * \internal
 * \brief Schema version of a secrets file with plain values, still read.
 */
constexpr int kPlainSecretsSchemaVersion = 1;

/*!
 * \internal
 * \brief Protection named in an encrypted secrets file.
 */
constexpr QLatin1StringView kDpapiProtection("dpapi");

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
} // namespace

/*!
 * \brief Creates a store over \a directory.
 * \param directory The directory holding the secrets files.
 * \param cipher Encrypts and decrypts the values.
 */
SecretStore::SecretStore(QString directory, SecretCipher cipher)
    : m_directory(std::move(directory))
    , m_cipher(std::move(cipher))
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
 * \param data The workspace to complete; unchanged unless the result is Ok.
 * \return The outcome of reading the file.
 */
SecretStore::ReadStatus SecretStore::applyTo(WorkspaceData &data) const
{
    return read(data, false);
}

/*!
 * \brief Fills only the secret variables of \a data that have no value yet.
 *
 * This completes a workspace whose secrets could not be read when it was opened,
 * without replacing values the user has entered since.
 * \param data The workspace to complete; unchanged unless the result is Ok.
 * \return The outcome of reading the file.
 */
SecretStore::ReadStatus SecretStore::fillEmptyValues(WorkspaceData &data) const
{
    return read(data, true);
}

/*!
 * \internal
 * \brief Reads the secrets file into \a data.
 *
 * Rows with the same key take their values by position. A malformed file is set
 * aside, so it survives the next save; when even that fails the caller gets
 * Unreadable, like for a file that cannot be opened. An encrypted file that does
 * not decrypt is Unavailable and stays where it is: it most likely belongs to
 * another user or computer, and DPAPI cannot tell that from damage.
 * \param data The workspace to complete.
 * \param onlyEmpty Whether values already present in \a data are kept.
 * \return The outcome of reading the file.
 */
SecretStore::ReadStatus SecretStore::read(WorkspaceData &data, bool onlyEmpty) const
{
    if (!isValidWorkspaceId(data.id))
        return ReadStatus::Ok;

    const QString path = filePath(data.id);
    QFile file(path);
    if (!file.exists())
        return ReadStatus::Ok;
    if (!file.open(QIODevice::ReadOnly))
        return ReadStatus::Unreadable;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    const auto corrupt = [&path] {
        return setAsideCorruptFile(path) ? ReadStatus::Corrupt : ReadStatus::Unreadable;
    };
    const QJsonObject root = document.object();
    const int schema = root.value(QLatin1StringView("schema")).toInt();
    QJsonValue environments;
    if (!document.isObject()) {
        return corrupt();
    } else if (schema == kPlainSecretsSchemaVersion) {
        environments = root.value(QLatin1StringView("environments"));
    } else if (schema == kSecretsSchemaVersion) {
        const QJsonValue data = root.value(QLatin1StringView("data"));
        if (root.value(QLatin1StringView("protection")).toString() != kDpapiProtection || !data.isString())
            return corrupt();
        const QByteArray::FromBase64Result sealed =
            QByteArray::fromBase64Encoding(data.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!sealed)
            return corrupt();
        const std::optional<QByteArray> plain = m_cipher.unprotect(*sealed);
        if (!plain)
            return ReadStatus::Unavailable;
        environments = QJsonDocument::fromJson(*plain).object().value(QLatin1StringView("environments"));
    }
    if (!environments.isObject())
        return corrupt();

    const QJsonObject stored = environments.toObject();
    for (Environment &environment : data.environments) {
        const QJsonObject values = stored.value(environment.id).toObject();
        QHash<QString, int> occurrences;
        for (EnvironmentVariable &variable : environment.variables) {
            if (!variable.secret)
                continue;
            const int index = occurrences[variable.key]++;
            const QString value = values.value(variable.key).toArray().at(index).toString();
            if (!value.isEmpty() && (!onlyEmpty || variable.value.isEmpty()))
                variable.value = value;
        }
    }
    return ReadStatus::Ok;
}

/*!
 * \brief Replaces the stored values of \a data.
 *
 * Every secret variable counts, including disabled ones; an empty value keeps
 * its position so that rows sharing a key stay aligned. An environment without
 * any value is left out, and with none left at all the file is removed, so a
 * workspace without secrets leaves nothing behind. Keys and values are written
 * encrypted, inside a small JSON object that names the schema and protection.
 * \param data The workspace to store.
 * \param errorString Receives the reason on failure when it is not null.
 * \return True on success.
 */
bool SecretStore::save(const WorkspaceData &data, QString *errorString) const
{
    const QString writeError = QStringLiteral("The secret values could not be written.");
    if (!isValidWorkspaceId(data.id))
        return fail(errorString, writeError);

    const QJsonObject environments = valuesOf(data);
    const QString path = filePath(data.id);
    if (environments.isEmpty()) {
        if (QFile::exists(path) && !QFile::remove(path))
            return fail(errorString, writeError);
        return true;
    }

    QJsonObject values;
    values.insert(QLatin1StringView("environments"), environments);
    const std::optional<QByteArray> sealed = m_cipher.protect(QJsonDocument(values).toJson(QJsonDocument::Compact));
    if (!sealed)
        return fail(errorString, QStringLiteral("The secret values could not be encrypted."));

    QJsonObject root;
    root.insert(QLatin1StringView("schema"), kSecretsSchemaVersion);
    root.insert(QLatin1StringView("protection"), kDpapiProtection);
    root.insert(QLatin1StringView("data"), QString::fromLatin1(sealed->toBase64()));
    return writeJsonFile(path, root) || fail(errorString, writeError);
}

/*!
 * \brief Returns whether the secrets file of \a workspaceId is a plain file of schema 1.
 *
 * Such a file is read like any other; the caller saves it again to encrypt it.
 * \param workspaceId The UUID of the workspace.
 */
bool SecretStore::isPlain(const QString &workspaceId) const
{
    const std::optional<QByteArray> content = snapshot(workspaceId);
    return content
           && QJsonDocument::fromJson(*content).object().value(QLatin1StringView("schema")).toInt()
                  == kPlainSecretsSchemaVersion;
}

/*!
 * \brief Returns the secret values of \a data, per environment and key, as save() stores them.
 *
 * Environments without any value are left out, so two workspaces whose secrets
 * would give the same file compare equal.
 * \param data The workspace.
 */
QJsonObject SecretStore::valuesOf(const WorkspaceData &data)
{
    QJsonObject environments;
    for (const Environment &environment : data.environments) {
        QJsonObject values;
        bool hasValue = false;
        for (const EnvironmentVariable &variable : environment.variables) {
            if (!variable.secret)
                continue;
            QJsonArray rows = values.value(variable.key).toArray();
            rows.append(variable.value);
            values.insert(variable.key, rows);
            hasValue = hasValue || !variable.value.isEmpty();
        }
        if (hasValue)
            environments.insert(environment.id, values);
    }
    return environments;
}

/*!
 * \brief Returns the values of \a data that the secrets file holds.
 *
 * A value kept in the workspace file instead, or entered since, is not among
 * them, so comparing with valuesOf() tells whether the file must be written.
 * \param data The workspace.
 * \return The values as valuesOf() gives them; empty when the file does not read Ok.
 */
QJsonObject SecretStore::storedValues(const WorkspaceData &data) const
{
    WorkspaceData fromFile = data;
    for (Environment &environment : fromFile.environments) {
        for (EnvironmentVariable &variable : environment.variables) {
            if (variable.secret)
                variable.value.clear();
        }
    }
    return read(fromFile, false) == ReadStatus::Ok ? valuesOf(fromFile) : QJsonObject();
}

/*!
 * \brief Returns the secrets file of \a workspaceId in a form safe to copy.
 *
 * An encrypted file is returned as it is. A plain file of schema 1 is returned
 * encrypted, with every value it holds, also those no variable uses any more,
 * so a copy never keeps a plain value.
 * \param workspaceId The UUID of the workspace.
 * \param content Receives the content, or std::nullopt when there is no file.
 * \return False when the file exists but cannot be read or encrypted.
 */
bool SecretStore::sealedSnapshot(const QString &workspaceId, std::optional<QByteArray> &content) const
{
    content = snapshot(workspaceId);
    if (!content)
        return !QFile::exists(filePath(workspaceId));
    const QJsonObject root = QJsonDocument::fromJson(*content).object();
    if (root.value(QLatin1StringView("schema")).toInt() != kPlainSecretsSchemaVersion)
        return true;

    QJsonObject values;
    values.insert(QLatin1StringView("environments"), root.value(QLatin1StringView("environments")));
    const std::optional<QByteArray> sealed = m_cipher.protect(QJsonDocument(values).toJson(QJsonDocument::Compact));
    if (!sealed)
        return false;
    QJsonObject wrapper;
    wrapper.insert(QLatin1StringView("schema"), kSecretsSchemaVersion);
    wrapper.insert(QLatin1StringView("protection"), kDpapiProtection);
    wrapper.insert(QLatin1StringView("data"), QString::fromLatin1(sealed->toBase64()));
    content = QJsonDocument(wrapper).toJson(QJsonDocument::Indented);
    return true;
}

/*!
 * \brief Returns the content of the secrets file of \a workspaceId.
 * \param workspaceId The UUID of the workspace.
 * \return The content, or std::nullopt when the file is missing or unreadable.
 */
std::optional<QByteArray> SecretStore::snapshot(const QString &workspaceId) const
{
    QFile file(filePath(workspaceId));
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return file.readAll();
}

/*!
 * \brief Puts the secrets file of \a workspaceId back to a snapshot().
 * \param workspaceId The UUID of the workspace.
 * \param content The snapshot; std::nullopt removes the file.
 * \return True on success.
 */
bool SecretStore::restore(const QString &workspaceId, const std::optional<QByteArray> &content) const
{
    const QString path = filePath(workspaceId);
    if (content)
        return writeFileAtomically(path, *content);
    return !QFile::exists(path) || QFile::remove(path);
}
