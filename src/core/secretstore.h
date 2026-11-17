#ifndef SECRETSTORE_H
#define SECRETSTORE_H

#include <QString>

// Encrypts / decrypts small secrets (e.g. SSH passwords) before they are
// written to sessions.json, so they are never stored in plain text.
//
// On Windows the implementation uses the DPAPI (CryptProtectData /
// CryptUnprotectData): the ciphertext is bound to the current Windows user
// account and can only be decrypted by that same user on that machine.
//
// On other platforms (Linux / macOS) it falls back to a lightweight XOR +
// Base64 obfuscation — not strong cryptography, but it keeps the password from
// appearing as readable plain text in the JSON file.
//
// Stored values are tagged with a short prefix so loading can transparently
// handle legacy plain-text entries and both encryption schemes.
namespace SecretStore {

// Encrypt a plaintext secret into a tagged, file-safe string.
QString encrypt(const QString &plain);

// Decrypt a value produced by encrypt(). If the value is untagged (legacy
// plain text) it is returned unchanged, so existing sessions.json files keep
// working and get upgraded on the next save.
QString decrypt(const QString &stored);

} // namespace SecretStore

#endif // SECRETSTORE_H