// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OAUTHTOKENSTORE_H
#define OAUTHTOKENSTORE_H

#include <QHash>
#include <QObject>
#include <QString>

#include <optional>

#include "core/http/oauthclient.h"
#include "core/workspace/secretcipher.h"

/**
 * Keeps the OAuth 2.0 tokens of one workspace.
 *
 * A token belongs to the node whose settings got it and to those settings: the
 * key holds a hash of them, so a changed setting never sends an old token.
 * The tokens can be saved to a file encrypted for the Windows user.
 */
class OAuthTokenStore : public QObject
{
    Q_OBJECT

public:
    /** Creates an empty store. */
    explicit OAuthTokenStore(QObject *parent = nullptr);

    /** Returns the key of the token that \a settings, resolved, get for the node \a nodeId. */
    static QString keyFor(const QString &nodeId, const OAuth2Settings &settings);

    /** Returns the token kept under \a key, or nothing. */
    std::optional<OAuthToken> token(const QString &key) const;

    /** Keeps \a token under \a key; \a fromRefresh keeps the old refresh token when the new one has none. */
    void setToken(const QString &key, const OAuthToken &token, bool fromRefresh = false);

    /** Removes every token of the node \a nodeId; returns whether there was one. */
    bool removeNode(const QString &nodeId);

    /** Returns whether a token of the node \a nodeId is kept. */
    bool hasNode(const QString &nodeId) const;

    /** Removes every token. */
    void clear();

    /** Replaces the tokens with those saved at \a path; a missing file means none. */
    bool load(const QString &path, const SecretCipher &cipher, QString *errorString = nullptr);

    /** Saves the tokens to \a path encrypted; without any the file is removed. */
    bool save(const QString &path, const SecretCipher &cipher, QString *errorString = nullptr) const;

signals:
    /** Emitted after the tokens changed. */
    void changed();

private:
    /** Tokens by key. */
    QHash<QString, OAuthToken> m_tokens;
};

#endif // OAUTHTOKENSTORE_H
