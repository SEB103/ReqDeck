// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SECRETCIPHER_H
#define SECRETCIPHER_H

#include <QByteArray>

#include <optional>

/**
 * Encrypts data for the current Windows user with DPAPI.
 *
 * Only the same user on the same computer can decrypt it again. The entropy is
 * a fixed string that keeps other programs of that user from decrypting the
 * data by accident; it is not a secret. Without DPAPI every call fails.
 */
class SecretCipher
{
public:
    /** Creates a cipher with \a entropy; tests pass another one to act as another computer. */
    explicit SecretCipher(QByteArray entropy = defaultEntropy());

    /** Returns the entropy ReqDeck uses for its secret values. */
    static QByteArray defaultEntropy();

    /** Returns \a plain encrypted, or nullopt when the system refused. */
    std::optional<QByteArray> protect(const QByteArray &plain) const;

    /** Returns \a sealed decrypted, or nullopt when it belongs to another user, key, or is damaged. */
    std::optional<QByteArray> unprotect(const QByteArray &sealed) const;

private:
    /** Additional input both calls must agree on. */
    QByteArray m_entropy;
};

#endif // SECRETCIPHER_H
