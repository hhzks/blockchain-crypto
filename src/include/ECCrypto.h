#pragma once

#include "bigint.h"
#include "sha.h"
#include <string>
#include <vector>
#include <memory>
#include <array>

namespace ECCrypto {

namespace secp256k1 {
    const BigInt P("115792089237316195423570985008687907853269984665640564039457584007908834671663");
    const BigInt A(0);
    const BigInt B(7);
    const BigInt GX("55066263022277343669578718895168534326250603453777594175500187360389116729240");
    const BigInt GY("32670510020758816978083085130507043184471273380659243275938904335757337482424");
    const BigInt N("115792089237316195423570985008687907852837564279074904382605163141518161494337");
}

constexpr size_t PRIVATE_KEY_SIZE = 32;
constexpr size_t PUBLIC_KEY_SIZE = 33;
constexpr size_t UNCOMPRESSED_PUBLIC_KEY_SIZE = 65;
constexpr size_t SIGNATURE_SIZE = 64;
constexpr size_t HASH_SIZE = 32;

constexpr uint8_t COMPRESSED_EVEN_PREFIX = 0x02;
constexpr uint8_t COMPRESSED_ODD_PREFIX = 0x03;
constexpr uint8_t UNCOMPRESSED_PREFIX = 0x04;

using Hash = std::array<uint8_t, HASH_SIZE>;
using Signature = std::array<uint8_t, SIGNATURE_SIZE>;
using PublicKey = std::array<uint8_t, PUBLIC_KEY_SIZE>;
using UncompressedPublicKey = std::array<uint8_t, UNCOMPRESSED_PUBLIC_KEY_SIZE>;
using PrivateKey = std::array<uint8_t, PRIVATE_KEY_SIZE>;

class ECPoint {
private:
    BigInt x, y, p, a, b;
    bool is_infinity;

public:
    ECPoint(BigInt gx, BigInt gy, BigInt prime, BigInt curve_a, BigInt curve_b)
        : x(gx), y(gy), p(prime), a(curve_a), b(curve_b), is_infinity(false) {}

    ECPoint& operator=(const ECPoint& other) = default;

    BigInt getX() const;
    BigInt getY() const;
    bool isPointAtInfinity() const;

    ECPoint pointAtInfinity() const;
    ECPoint doubledPoint() const;
    ECPoint operator+(const ECPoint& other) const;
    ECPoint operator*(const BigInt& scalar) const;
    ECPoint operator+=(const ECPoint& other);
    ECPoint operator*=(const BigInt& scalar);
};

inline const ECPoint G{secp256k1::GX, secp256k1::GY, secp256k1::P, secp256k1::A, secp256k1::B};

struct KeyPair {
    BigInt private_key;
    ECPoint public_key;
    std::string private_key_hex;
    std::string public_key_hex;
    std::string address;

    KeyPair();
    KeyPair(const BigInt& priv_key, const ECPoint& pub_key);
};

std::unique_ptr<KeyPair> generateKeyPair();
std::unique_ptr<KeyPair> keyPairFromPrivateKey(const BigInt& private_key);
std::unique_ptr<KeyPair> keyPairFromPrivateKeyHex(const std::string& private_key_hex);

Signature signHash(const Hash& hash, const BigInt& private_key);
Signature signHash(const Hash& hash, const PrivateKey& private_key);
Signature signMessage(const std::string& message, const BigInt& private_key);
Signature signMessage(const std::string& message, const PrivateKey& private_key);

bool verifySignature(const Hash& hash, const Signature& signature, const PublicKey& public_key);
bool verifyMessageSignature(const std::string& message, const Signature& signature, const PublicKey& public_key);

std::string deriveAddress(const PublicKey& public_key);
std::string bytesToHex(const uint8_t* data, size_t size);
size_t hexToBytes(const std::string& hex, uint8_t* output, size_t max_size);

Hash sha256Hash(const std::string& message);
Hash sha256Hash(const uint8_t* data, size_t size);

bool isValidPrivateKey(const PrivateKey& private_key);
bool isValidPublicKey(const PublicKey& public_key);

std::string signatureToHex(const Signature& signature);
Signature signatureFromHex(const std::string& hex);

bool testECCImplementation();

PublicKey compressPublicKey(const ECPoint& point);
ECPoint decompressPublicKey(const PublicKey& compressed_key);
UncompressedPublicKey toUncompressedPublicKey(const ECPoint& point);
ECPoint fromUncompressedPublicKey(const UncompressedPublicKey& uncompressed_key);
PublicKey compressPublicKey(const UncompressedPublicKey& uncompressed_key);
UncompressedPublicKey decompressToUncompressed(const PublicKey& compressed_key);

bool isCompressedPublicKey(const uint8_t* data, size_t size);
bool isUncompressedPublicKey(const uint8_t* data, size_t size);

void bigIntToBytes32(const BigInt& num, uint8_t* output);
BigInt bytes32ToBigInt(const uint8_t* data);
BigInt modSqrt(const BigInt& n, const BigInt& p);

}
