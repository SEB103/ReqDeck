// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/http/oauthtokenstore.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/workspace/jsonfile.h"

namespace {
/*!
 * \internal
 * \brief Version of the tokens file.
 */
constexpr int kTokensSchemaVersion = 1;

/*!
 * \internal
 * \brief Protection name in the tokens file: DPAPI for the current user.
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
 * \class OAuthTokenStore
 * \inmodule ReqDeckCore
 * \brief Keeps the OAuth 2.0 tokens of one workspace.
 */

/*!
 * \brief Creates an empty store owned by \a parent.
 */
OAuthTokenStore::OAuthTokenStore(QObject *parent)
    : QObject(parent)
{
}

/*!
 * \brief Returns the key of the token that \a settings get for the node \a nodeId.
 *
 * "<nodeId>/<hash>", where the hash covers everything that decides which
 * token the server gives. The client secret is left out: it may be a
 * one-time value at a send, and a token got with an older secret stays
 * valid at the server anyway.
 * \param nodeId The request or folder whose auth holds the settings.
 * \param settings The settings with variables resolved.
 */
QString OAuthTokenStore::keyFor(const QString &nodeId, const OAuth2Settings &settings)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const QString &part : {QString::number(int(settings.grant)), settings.authUrl.trimmed(),
                                settings.tokenUrl.trimmed(), settings.clientId.trimmed(), settings.scope.trimmed(),
                                settings.audience.trimmed()}) {
        hash.addData(part.toUtf8());
        hash.addData(QByteArrayView("\x1f", 1));
    }
    return nodeId + u'/' + QString::fromLatin1(hash.result().toHex().left(32));
}

/*!
 * \brief Returns the token kept under \a key, or nothing.
 */
std::optional<OAuthToken> OAuthTokenStore::token(const QString &key) const
{
    const auto it = m_tokens.constFind(key);
    if (it == m_tokens.constEnd())
        return std::nullopt;
    return *it;
}

/*!
 * \brief Keeps \a token under \a key, replacing the token kept before.
 *
 * After a refresh without a new refresh token the old one stays, as RFC 6749
 * section 6 allows the server to leave it out; a new login replaces it.
 * \param key The key from keyFor().
 * \param token The token.
 * \param fromRefresh Whether the token came from a refresh.
 */
void OAuthTokenStore::setToken(const QString &key, const OAuthToken &token, bool fromRefresh)
{
    OAuthToken kept = token;
    if (fromRefresh && kept.refreshToken.isEmpty()) {
        if (const auto old = m_tokens.constFind(key); old != m_tokens.constEnd())
            kept.refreshToken = old->refreshToken;
    }
    m_tokens.insert(key, kept);
    emit changed();
}

/*!
 * \brief Removes every token of the node \a nodeId, as Reset does.
 * \return Whether a token was removed.
 */
bool OAuthTokenStore::removeNode(const QString &nodeId)
{
    const QString prefix = nodeId + u'/';
    const qsizetype removed = m_tokens.removeIf([&prefix](const auto &entry) { return entry.key().startsWith(prefix); });
    if (removed > 0)
        emit changed();
    return removed > 0;
}

/*!
 * \brief Returns whether a token of the node \a nodeId is kept.
 */
bool OAuthTokenStore::hasNode(const QString &nodeId) const
{
    const QString prefix = nodeId + u'/';
    for (auto it = m_tokens.cbegin(); it != m_tokens.cend(); ++it) {
        if (it.key().startsWith(prefix))
            return true;
    }
    return false;
}

/*!
 * \brief Removes every token.
 */
void OAuthTokenStore::clear()
{
    if (m_tokens.isEmpty())
        return;
    m_tokens.clear();
    emit changed();
}

/*!
 * \brief Replaces the tokens with those saved at \a path.
 *
 * The store is emptied first, also when reading fails. A missing file is an
 * empty store.
 * \param path The tokens file of the workspace.
 * \param cipher The cipher that encrypted it.
 * \param errorString Receives the reason of a failure; may be null.
 * \return False when the file exists but cannot be read or decrypted.
 */
bool OAuthTokenStore::load(const QString &path, const SecretCipher &cipher, QString *errorString)
{
    m_tokens.clear();
    emit changed();
    QFile file(path);
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly))
        return fail(errorString, QStringLiteral("The OAuth tokens could not be read."));
    const QJsonObject root = QJsonDocument::fromJson(file.read(16 * 1024 * 1024)).object();
    if (root.value(QLatin1StringView("schema")).toInt() != kTokensSchemaVersion
        || root.value(QLatin1StringView("protection")).toString() != kDpapiProtection)
        return fail(errorString, QStringLiteral("The OAuth tokens file is not one this version reads."));
    const std::optional<QByteArray> plain =
        cipher.unprotect(QByteArray::fromBase64(root.value(QLatin1StringView("data")).toString().toLatin1()));
    if (!plain)
        return fail(errorString, QStringLiteral("The OAuth tokens were saved for another Windows user or computer."));
    const QJsonObject tokens = QJsonDocument::fromJson(*plain).object();
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        const QJsonObject object = it.value().toObject();
        OAuthToken token;
        token.accessToken = object.value(QLatin1StringView("access")).toString();
        token.refreshToken = object.value(QLatin1StringView("refresh")).toString();
        token.tokenType = object.value(QLatin1StringView("type")).toString();
        token.scope = object.value(QLatin1StringView("scope")).toString();
        token.expiresAt = QDateTime::fromString(object.value(QLatin1StringView("expires")).toString(), Qt::ISODate);
        if (!token.accessToken.isEmpty() || !token.refreshToken.isEmpty())
            m_tokens.insert(it.key(), token);
    }
    emit changed();
    return true;
}

/*!
 * \brief Saves the tokens to \a path, encrypted for the Windows user.
 *
 * Without any token the file is removed.
 * \param path The tokens file of the workspace.
 * \param cipher The cipher to encrypt with.
 * \param errorString Receives the reason of a failure; may be null.
 * \return True on success.
 */
bool OAuthTokenStore::save(const QString &path, const SecretCipher &cipher, QString *errorString) const
{
    if (m_tokens.isEmpty()) {
        if (QFile::exists(path) && !QFile::remove(path))
            return fail(errorString, QStringLiteral("The OAuth tokens could not be written."));
        return true;
    }
    QJsonObject tokens;
    for (auto it = m_tokens.cbegin(); it != m_tokens.cend(); ++it) {
        tokens.insert(it.key(), QJsonObject {{QStringLiteral("access"), it->accessToken},
                                             {QStringLiteral("refresh"), it->refreshToken},
                                             {QStringLiteral("type"), it->tokenType},
                                             {QStringLiteral("scope"), it->scope},
                                             {QStringLiteral("expires"), it->expiresAt.toString(Qt::ISODateWithMs)}});
    }
    const std::optional<QByteArray> sealed = cipher.protect(QJsonDocument(tokens).toJson(QJsonDocument::Compact));
    if (!sealed)
        return fail(errorString, QStringLiteral("The OAuth tokens could not be encrypted."));
    QJsonObject root;
    root.insert(QLatin1StringView("schema"), kTokensSchemaVersion);
    root.insert(QLatin1StringView("protection"), kDpapiProtection);
    root.insert(QLatin1StringView("data"), QString::fromLatin1(sealed->toBase64()));
    return writeJsonFile(path, root) || fail(errorString, QStringLiteral("The OAuth tokens could not be written."));
}
