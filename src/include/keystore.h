#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Password-based encryption for the wallet keystore.
//
// The primitives here are built on this project's own SHA-256, because it has
// no crypto library and no AES. The construction is standard --
// PBKDF2-HMAC-SHA256 for the key, an HMAC-SHA256 counter-mode keystream for
// confidentiality, and encrypt-then-MAC with a separate key for integrity --
// but the code underneath it is hand-rolled and unaudited, like the rest of
// the crypto in this project. It raises the cost of reading a stolen keystore
// from "open the file" to "run a PBKDF2 dictionary attack"; it is not a
// substitute for a reviewed implementation.
namespace keystore {

using Mac = std::array<uint8_t, 32>;

constexpr size_t SALT_SIZE = 16;
constexpr size_t NONCE_SIZE = 16;
constexpr size_t KEY_SIZE = 32;

// Iteration count for new files. The count actually used to decrypt is read
// from the file, so raising this stays backwards compatible.
constexpr uint32_t DEFAULT_ITERATIONS = 200000;

// HMAC-SHA256 (RFC 2104).
Mac hmacSha256(const uint8_t* key, size_t key_len,
               const uint8_t* data, size_t data_len);

// PBKDF2-HMAC-SHA256 (RFC 2898).
std::vector<uint8_t> pbkdf2Sha256(const std::string& password,
                                  const std::vector<uint8_t>& salt,
                                  uint32_t iterations,
                                  size_t output_length);

// Encrypts `plaintext` under `password`, returning a self-describing text
// blob: a version line, then the salt, iteration count, nonce, tag and
// ciphertext as hex. Returns an empty string if the OS could not supply
// entropy for the salt and nonce.
// `iterations` is a cost knob: the count used is written into the blob, so
// decrypt honours whatever a file was made with and raising the default stays
// backwards compatible.
std::string encrypt(const std::string& plaintext, const std::string& password,
                    uint32_t iterations = DEFAULT_ITERATIONS);

// Returns the plaintext, or nullopt if the blob is malformed, the password is
// wrong, or the contents were tampered with. The three are deliberately
// indistinguishable to the caller.
std::optional<std::string> decrypt(const std::string& armored,
                                   const std::string& password);

} // namespace keystore
