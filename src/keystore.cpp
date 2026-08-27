#include "include/keystore.h"
#include "include/ECCrypto.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace keystore {

namespace {

constexpr size_t BLOCK_SIZE = 64; // SHA-256 compression block
constexpr std::string_view FORMAT_TAG = "KEYSTORE1";

Mac sha256Of(const std::vector<uint8_t>& data) {
    ECCrypto::Hash hash = ECCrypto::sha256Hash(data.data(), data.size());
    Mac result{};
    std::copy(hash.begin(), hash.end(), result.begin());
    return result;
}

void appendU32BE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

// Compares in time independent of where the first difference is, so a
// tampered tag cannot be repaired byte by byte from timing.
bool constantTimeEquals(const Mac& a, const Mac& b) {
    uint8_t difference = 0;
    for (size_t i = 0; i < a.size(); i++) {
        difference |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return difference == 0;
}

std::string toHex(const std::vector<uint8_t>& data) {
    return ECCrypto::bytesToHex(data.data(), data.size());
}

std::optional<std::vector<uint8_t>> fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> out(hex.size() / 2);
    if (!out.empty() &&
        ECCrypto::hexToBytes(hex, out.data(), out.size()) != out.size()) {
        return std::nullopt;
    }
    return out;
}

// Keystream block i = HMAC(key, nonce || i), which is the standard way to
// build a stream cipher out of a PRF when there is no block cipher available.
void applyKeystream(const std::vector<uint8_t>& key,
                    const std::vector<uint8_t>& nonce,
                    std::vector<uint8_t>& data) {
    std::vector<uint8_t> input;
    input.reserve(nonce.size() + 4);

    for (size_t offset = 0; offset < data.size(); offset += KEY_SIZE) {
        input.assign(nonce.begin(), nonce.end());
        appendU32BE(input, static_cast<uint32_t>(offset / KEY_SIZE));

        Mac block = hmacSha256(key.data(), key.size(), input.data(), input.size());

        const size_t span = std::min(KEY_SIZE, data.size() - offset);
        for (size_t i = 0; i < span; i++) {
            data[offset + i] ^= block[i];
        }
    }
}

} // namespace

Mac hmacSha256(const uint8_t* key, size_t key_len,
               const uint8_t* data, size_t data_len) {
    std::vector<uint8_t> padded_key(BLOCK_SIZE, 0);

    if (key_len > BLOCK_SIZE) {
        // RFC 2104: a key longer than the block size is hashed first.
        ECCrypto::Hash hashed = ECCrypto::sha256Hash(key, key_len);
        std::copy(hashed.begin(), hashed.end(), padded_key.begin());
    } else {
        std::copy(key, key + key_len, padded_key.begin());
    }

    std::vector<uint8_t> inner;
    inner.reserve(BLOCK_SIZE + data_len);
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        inner.push_back(static_cast<uint8_t>(padded_key[i] ^ 0x36));
    }
    inner.insert(inner.end(), data, data + data_len);

    Mac inner_hash = sha256Of(inner);

    std::vector<uint8_t> outer;
    outer.reserve(BLOCK_SIZE + inner_hash.size());
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        outer.push_back(static_cast<uint8_t>(padded_key[i] ^ 0x5C));
    }
    outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());

    return sha256Of(outer);
}

std::vector<uint8_t> pbkdf2Sha256(const std::string& password,
                                  const std::vector<uint8_t>& salt,
                                  uint32_t iterations,
                                  size_t output_length) {
    std::vector<uint8_t> output;
    output.reserve(output_length);

    const auto* key = reinterpret_cast<const uint8_t*>(password.data());
    const size_t key_len = password.size();

    // Blocks are 1-indexed: T_n = F(P, S, c, n), F = U_1 ^ ... ^ U_c.
    for (uint32_t block = 1; output.size() < output_length; block++) {
        std::vector<uint8_t> input(salt);
        appendU32BE(input, block);

        Mac u = hmacSha256(key, key_len, input.data(), input.size());
        Mac accumulator = u;

        for (uint32_t i = 1; i < iterations; i++) {
            u = hmacSha256(key, key_len, u.data(), u.size());
            for (size_t j = 0; j < accumulator.size(); j++) {
                accumulator[j] ^= u[j];
            }
        }

        const size_t span = std::min(accumulator.size(), output_length - output.size());
        output.insert(output.end(), accumulator.begin(), accumulator.begin() + span);
    }

    return output;
}

std::string encrypt(const std::string& plaintext, const std::string& password,
                    uint32_t iterations) {
    std::vector<uint8_t> salt(SALT_SIZE);
    std::vector<uint8_t> nonce(NONCE_SIZE);

    try {
        ECCrypto::secureRandomBytes(salt.data(), salt.size());
        ECCrypto::secureRandomBytes(nonce.data(), nonce.size());
    } catch (const std::exception&) {
        return {}; // no entropy, no keystore
    }

    // One PBKDF2 run, split into an encryption key and a separate MAC key, so
    // the keystream and the tag never share key material.
    if (iterations == 0) {
        return {};
    }

    std::vector<uint8_t> derived =
        pbkdf2Sha256(password, salt, iterations, KEY_SIZE * 2);
    std::vector<uint8_t> enc_key(derived.begin(), derived.begin() + KEY_SIZE);
    std::vector<uint8_t> mac_key(derived.begin() + KEY_SIZE, derived.end());

    std::vector<uint8_t> ciphertext(plaintext.begin(), plaintext.end());
    applyKeystream(enc_key, nonce, ciphertext);

    // Encrypt-then-MAC over the header as well as the ciphertext, so the salt,
    // iteration count and nonce cannot be swapped either.
    std::ostringstream header;
    header << FORMAT_TAG << "\n"
           << toHex(salt) << "\n"
           << iterations << "\n"
           << toHex(nonce) << "\n";

    // One string, held: header.str() returns a fresh temporary each call, so
    // taking begin() from one and end() from another is undefined.
    const std::string header_text = header.str();

    std::vector<uint8_t> authenticated(header_text.begin(), header_text.end());
    authenticated.insert(authenticated.end(), ciphertext.begin(), ciphertext.end());

    Mac tag = hmacSha256(mac_key.data(), mac_key.size(),
                         authenticated.data(), authenticated.size());

    std::ostringstream out;
    out << header_text
        << ECCrypto::bytesToHex(tag.data(), tag.size()) << "\n"
        << toHex(ciphertext) << "\n";
    return out.str();
}

std::optional<std::string> decrypt(const std::string& armored,
                                   const std::string& password) {
    std::istringstream in(armored);
    std::string format, salt_hex, iterations_text, nonce_hex, tag_hex, ciphertext_hex;

    if (!std::getline(in, format) || format != FORMAT_TAG) {
        return std::nullopt;
    }
    if (!std::getline(in, salt_hex) || !std::getline(in, iterations_text) ||
        !std::getline(in, nonce_hex) || !std::getline(in, tag_hex)) {
        return std::nullopt;
    }
    // An empty keystore is legitimate, so a missing final line is not.
    if (!std::getline(in, ciphertext_hex)) {
        return std::nullopt;
    }

    uint32_t iterations = 0;
    {
        std::istringstream parse(iterations_text);
        if (!(parse >> iterations) || iterations == 0) {
            return std::nullopt;
        }
    }

    auto salt = fromHex(salt_hex);
    auto nonce = fromHex(nonce_hex);
    auto tag_bytes = fromHex(tag_hex);
    auto ciphertext = fromHex(ciphertext_hex);

    if (!salt || !nonce || !tag_bytes || !ciphertext) {
        return std::nullopt;
    }
    if (salt->size() != SALT_SIZE || nonce->size() != NONCE_SIZE ||
        tag_bytes->size() != KEY_SIZE) {
        return std::nullopt;
    }

    std::vector<uint8_t> derived =
        pbkdf2Sha256(password, *salt, iterations, KEY_SIZE * 2);
    std::vector<uint8_t> enc_key(derived.begin(), derived.begin() + KEY_SIZE);
    std::vector<uint8_t> mac_key(derived.begin() + KEY_SIZE, derived.end());

    std::ostringstream header;
    header << FORMAT_TAG << "\n"
           << salt_hex << "\n"
           << iterations_text << "\n"
           << nonce_hex << "\n";

    const std::string header_text = header.str();

    std::vector<uint8_t> authenticated(header_text.begin(), header_text.end());
    authenticated.insert(authenticated.end(), ciphertext->begin(), ciphertext->end());

    Mac expected = hmacSha256(mac_key.data(), mac_key.size(),
                              authenticated.data(), authenticated.size());
    Mac actual{};
    std::copy(tag_bytes->begin(), tag_bytes->end(), actual.begin());

    // Verify before decrypting: a wrong password and a tampered file are the
    // same failure here, and neither reveals which it was.
    if (!constantTimeEquals(expected, actual)) {
        return std::nullopt;
    }

    applyKeystream(enc_key, *nonce, *ciphertext);
    return std::string(ciphertext->begin(), ciphertext->end());
}

} // namespace keystore
