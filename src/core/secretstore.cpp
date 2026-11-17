#include "secretstore.h"

#include <QByteArray>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <wincrypt.h>
#endif

namespace {

// Tags identifying how a stored value was produced.
const QString kDpapiTag = QStringLiteral("dpapi:");   // Windows DPAPI
const QString kXorTag   = QStringLiteral("x1:");      // XOR + Base64 fallback

// Fixed key for the cross-platform XOR fallback. This is obfuscation, not
// strong crypto — it only prevents casual plain-text reading of the file.
const QByteArray kXorKey = QByteArrayLiteral("TerminalPP::SecretXorKey::v1");

QByteArray xorBytes(const QByteArray &in)
{
    QByteArray out = in;
    for (int i = 0; i < out.size(); ++i)
        out[i] = out[i] ^ kXorKey[i % kXorKey.size()];
    return out;
}

} // namespace

namespace SecretStore {

QString encrypt(const QString &plain)
{
    if (plain.isEmpty())
        return QString();

    const QByteArray data = plain.toUtf8();

#ifdef Q_OS_WIN
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(data.constData()));
    in.cbData = static_cast<DWORD>(data.size());

    DATA_BLOB out;
    out.pbData = nullptr;
    out.cbData = 0;

    // CRYPTPROTECT_UI_FORBIDDEN: never prompt; purely silent per-user encryption.
    if (CryptProtectData(&in, L"TerminalPP SSH password", nullptr, nullptr,
                         nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        QByteArray cipher(reinterpret_cast<const char *>(out.pbData),
                          static_cast<int>(out.cbData));
        if (out.pbData)
            LocalFree(out.pbData);
        return kDpapiTag + QString::fromLatin1(cipher.toBase64());
    }
    // If DPAPI fails for any reason, fall through to the XOR fallback below.
#endif

    return kXorTag + QString::fromLatin1(xorBytes(data).toBase64());
}

QString decrypt(const QString &stored)
{
    if (stored.isEmpty())
        return QString();

#ifdef Q_OS_WIN
    if (stored.startsWith(kDpapiTag)) {
        const QByteArray cipher =
            QByteArray::fromBase64(stored.mid(kDpapiTag.size()).toLatin1());

        DATA_BLOB in;
        in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(cipher.constData()));
        in.cbData = static_cast<DWORD>(cipher.size());

        DATA_BLOB out;
        out.pbData = nullptr;
        out.cbData = 0;

        if (CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                               CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            QByteArray plain(reinterpret_cast<const char *>(out.pbData),
                             static_cast<int>(out.cbData));
            if (out.pbData)
                LocalFree(out.pbData);
            return QString::fromUtf8(plain);
        }
        // Decryption failed (e.g. different user/machine) — return empty.
        return QString();
    }
#endif

    if (stored.startsWith(kXorTag)) {
        const QByteArray data =
            QByteArray::fromBase64(stored.mid(kXorTag.size()).toLatin1());
        return QString::fromUtf8(xorBytes(data));
    }

    // Legacy / untagged value: treat as plain text (upgraded on next save).
    return stored;
}

} // namespace SecretStore