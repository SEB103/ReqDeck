// SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/workspace/secretcipher.h"

#include <QtGlobal>

#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dpapi.h>
#endif

#ifdef Q_OS_WIN
namespace {
/*!
 * \internal
 * \brief Returns a DPAPI blob that points at the bytes of \a data.
 *
 * DPAPI only reads input blobs, so the cast away from const is safe.
 */
DATA_BLOB blobOf(const QByteArray &data)
{
    return DATA_BLOB {DWORD(data.size()), reinterpret_cast<BYTE *>(const_cast<char *>(data.constData()))};
}

/*!
 * \internal
 * \brief Copies the output blob \a blob and frees the memory DPAPI allocated for it.
 */
QByteArray takeBlob(DATA_BLOB &blob)
{
    QByteArray result(reinterpret_cast<const char *>(blob.pbData), qsizetype(blob.cbData));
    SecureZeroMemory(blob.pbData, blob.cbData);
    LocalFree(blob.pbData);
    blob = {};
    return result;
}
} // namespace
#endif

/*!
 * \brief Creates a cipher that uses \a entropy.
 * \param entropy Additional input; data protected with one entropy needs the same to unprotect.
 */
SecretCipher::SecretCipher(QByteArray entropy)
    : m_entropy(std::move(entropy))
{
}

/*!
 * \brief Returns the entropy ReqDeck uses for its secret values.
 */
QByteArray SecretCipher::defaultEntropy()
{
    return QByteArrayLiteral("ReqDeck secret values");
}

/*!
 * \brief Encrypts \a plain for the current user.
 *
 * CRYPTPROTECT_UI_FORBIDDEN makes the call fail instead of showing a dialog,
 * which a background save must never do.
 * \param plain The data to protect.
 * \return The encrypted data, or std::nullopt on failure.
 */
std::optional<QByteArray> SecretCipher::protect(const QByteArray &plain) const
{
#ifdef Q_OS_WIN
    DATA_BLOB input = blobOf(plain);
    DATA_BLOB entropy = blobOf(m_entropy);
    DATA_BLOB output {};
    if (!CryptProtectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        return std::nullopt;
    return takeBlob(output);
#else
    Q_UNUSED(plain);
    return std::nullopt;
#endif
}

/*!
 * \brief Decrypts \a sealed.
 *
 * Data of another user or computer, with another entropy, or damaged in any way
 * fails alike; DPAPI does not tell these cases apart reliably.
 * \param sealed Data from protect().
 * \return The plain data, or std::nullopt on failure.
 */
std::optional<QByteArray> SecretCipher::unprotect(const QByteArray &sealed) const
{
#ifdef Q_OS_WIN
    DATA_BLOB input = blobOf(sealed);
    DATA_BLOB entropy = blobOf(m_entropy);
    DATA_BLOB output {};
    if (!CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
        return std::nullopt;
    return takeBlob(output);
#else
    Q_UNUSED(sealed);
    return std::nullopt;
#endif
}
