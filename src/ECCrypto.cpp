#include "include/ECCrypto.h"
#include "include/sha.h"
#include "include/bigint.h"
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <bcrypt.h>
#else
    #include <fstream>
#endif

#include <iomanip>
#include <sstream>
#include <cstring>
#include <chrono>

namespace ECCrypto {

BigInt ECPoint::getX() const{
    return x;
}

BigInt ECPoint::getY() const{
    return y;
}

ECPoint ECPoint::pointAtInfinity() const {
    ECPoint result = ECPoint{0, 0, p, a, b};
    result.is_infinity = true;
    return result;
}

ECPoint ECPoint::doubledPoint() const{
    if (y == 0) {
        return pointAtInfinity();
    }

    BigInt slope = (((squared(x) * 3) + a) * inverse(y*2,p)) % p;
    BigInt new_x = (squared(slope) - (x * 2)) % p;
    BigInt new_y = (slope * (x - new_x) - y) % p;
    return ECPoint {new_x, new_y, p, a, b};
}

ECPoint ECPoint::operator+(const ECPoint& other) const {
    if (is_infinity && other.is_infinity) { return pointAtInfinity(); }
    if (!is_infinity && other.is_infinity) { return *this; }
    if (is_infinity && !other.is_infinity) { return other; }

    if (x == other.x && y == other.y){
        return doubledPoint();
    }

    if (x == other.x && y != other.y) {
        return pointAtInfinity();
    }

    BigInt slope = ((other.y - y) * inverse(other.x - x,p)) % p;
    BigInt new_x = (squared(slope) - x - other.x) % p;
    BigInt new_y = (slope * (x - new_x) - y) % p;
    return ECPoint {new_x, new_y, p, a, b};
}

ECPoint ECPoint::operator*(const BigInt& scalar) const{
    if (scalar == 0) {
        return pointAtInfinity();
    }
    
    ECPoint current = *this;
    std::string binary_str = scalar.to_binary_string();
    
    size_t i = 0;
    while (i < binary_str.size() && binary_str[i] != '1') {i++;}
    
    for (++i; i < binary_str.size(); i++){
        current = current.doubledPoint();
        if (binary_str[i] == '1') {
            current += *this;
        }

    }
    
    return current;
}

ECPoint ECPoint::operator+=(const ECPoint& other){
    *this = *this + other;
    return *this;
}

ECPoint ECPoint::operator*=(const BigInt& scalar){
    *this = *this * scalar;
    return *this;
}

void secureRandomBytes(uint8_t* output, size_t size) {
    if (size == 0) {
        return;
    }

#ifdef _WIN32
    NTSTATUS status = BCryptGenRandom(nullptr, output, static_cast<ULONG>(size),
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
        throw std::runtime_error("secureRandomBytes: BCryptGenRandom failed");
    }
#else
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom.read(reinterpret_cast<char*>(output),
                      static_cast<std::streamsize>(size))) {
        throw std::runtime_error("secureRandomBytes: /dev/urandom unavailable");
    }
#endif
}

BigInt randomScalar() {
    uint8_t bytes[PRIVATE_KEY_SIZE];

    // A draw outside [1, N-1] has probability under 2^-128, so this loop
    // effectively never runs twice; the bound is only there so a broken
    // entropy source fails loudly instead of spinning.
    for (int attempt = 0; attempt < 128; attempt++) {
        secureRandomBytes(bytes, sizeof bytes);
        BigInt candidate = bytes32ToBigInt(bytes);

        if (candidate > 0 && candidate < secp256k1::N) {
            return candidate;
        }
    }

    throw std::runtime_error("randomScalar: no valid scalar drawn");
}

std::unique_ptr<ECCrypto::KeyPair> generateKeyPair(){
    // 32 bytes straight from the OS CSPRNG. The previous version seeded a
    // std::mt19937_64 from a single 32-bit std::random_device draw and
    // assembled the key from unpadded hex, capping every key this program ever
    // produced at 2^32 possibilities and shortening it at random.
    return keyPairFromPrivateKey(randomScalar());
}

std::unique_ptr<KeyPair> keyPairFromPrivateKey(const BigInt& priv_key) {
    return std::make_unique<KeyPair>(priv_key, G * (priv_key % ECCrypto::secp256k1::N));
}

std::unique_ptr<KeyPair> keyPairFromPrivateKeyHex(const std::string& priv_key_hex) {
    BigInt priv_key{BigInt(priv_key_hex, 16)};
    return std::make_unique<KeyPair>(priv_key, G * (priv_key % ECCrypto::secp256k1::N));
}

bool ECPoint::isPointAtInfinity() const {
    return is_infinity;
}

// =====================================================
// KeyPair Constructors
// =====================================================

KeyPair::KeyPair() : public_key(G.pointAtInfinity()) {
    private_key = BigInt(0);
    private_key_hex = "";
    public_key_hex = "";
    address = "";
}

KeyPair::KeyPair(const BigInt& priv_key, const ECPoint& pub_key) 
    : private_key(priv_key), public_key(pub_key) {
    BigInt temp = priv_key;
    
    const char* hex_chars = "0123456789abcdef";
    private_key_hex = "";
    if (temp == 0) {
        private_key_hex = "0";
    } else {
        while (temp > 0) {
            BigInt remainder = temp % 16;
            private_key_hex = hex_chars[remainder.to_int()] + private_key_hex;
            temp = temp / 16;
        }
    }
    while (private_key_hex.length() < 64) {
        private_key_hex = "0" + private_key_hex;
    }
    
    PublicKey compressed = compressPublicKey(pub_key);
    public_key_hex = bytesToHex(compressed.data(), compressed.size());
    address = deriveAddress(compressed);
}

// =====================================================
// Utility Functions
// =====================================================

std::string bytesToHex(const uint8_t* data, size_t size) {
    std::stringstream ss;
    for (size_t i = 0; i < size; i++) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

namespace {

// Strict hex nibble. std::stoul was accepting leading whitespace, a leading
// sign, and -- because nobody checked how much it consumed -- any prefix of a
// valid number, so 64 characters of junk decoded to 32 "valid" key bytes.
constexpr int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

size_t hexToBytes(const std::string& hex, uint8_t* output, size_t maxSize) {
    if (hex.length() % 2 != 0) return 0;

    size_t byteCount = hex.length() / 2;
    if (byteCount > maxSize) return 0;

    for (size_t i = 0; i < byteCount; i++) {
        const int hi = hexNibble(hex[i * 2]);
        const int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return 0;
        output[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return byteCount;
}

Hash sha256Hash(const std::string& message) {
    std::string hashStr = SHA256::hash(message);
    Hash result = {};
    hexToBytes(hashStr, result.data(), HASH_SIZE);
    return result;
}

Hash sha256Hash(const uint8_t* data, size_t size) {
    std::string hashStr = SHA256::hash(data, size);
    Hash result = {};
    hexToBytes(hashStr, result.data(), HASH_SIZE);
    return result;
}

std::string deriveAddress(const PublicKey& publicKey) {
    // Hash the public key with SHA-256 and take first 20 bytes (40 hex chars)
    Hash hash = sha256Hash(publicKey.data(), publicKey.size());
    return bytesToHex(hash.data(), 20);  // Use first 20 bytes like Ethereum
}

bool isValidPrivateKey(const PrivateKey& privateKey) {
    BigInt key = bytes32ToBigInt(privateKey.data());
    // Private key must be > 0 and < N
    return key > 0 && key < secp256k1::N;
}

bool isValidPublicKey(const PublicKey& publicKey) {
    // Check prefix
    if (publicKey[0] != COMPRESSED_EVEN_PREFIX && publicKey[0] != COMPRESSED_ODD_PREFIX) {
        return false;
    }
    
    // Try to decompress - if it works, it's valid
    ECPoint point = decompressPublicKey(publicKey);
    return !point.isPointAtInfinity();
}

std::string signatureToHex(const Signature& signature) {
    return bytesToHex(signature.data(), signature.size());
}

Signature signatureFromHex(const std::string& hex) {
    Signature result = {};
    if (hex.length() != SIGNATURE_SIZE * 2) {
        return result;
    }
    hexToBytes(hex, result.data(), SIGNATURE_SIZE);
    return result;
}

Signature signHash(const Hash& hash, const BigInt& privateKey) {
    Signature result = {};

    // s = k + e*x, with e and R.x public, so recovering k recovers the private
    // key. The nonce must come from the OS CSPRNG, never from a userspace PRNG
    // whose whole state is derivable from 32 bits of seed.
    BigInt k = randomScalar();

    // R = k * G
    ECPoint R = G * k;
    
    // Get public key
    ECPoint pubKey = G * privateKey;
    
    // Compute e = SHA256(R.x || pubKey.x || hash)
    std::vector<uint8_t> toHash;
    uint8_t rxBytes[32], pubxBytes[32];
    bigIntToBytes32(R.getX(), rxBytes);
    bigIntToBytes32(pubKey.getX(), pubxBytes);
    
    for (int i = 0; i < 32; i++) toHash.push_back(rxBytes[i]);
    for (int i = 0; i < 32; i++) toHash.push_back(pubxBytes[i]);
    for (int i = 0; i < 32; i++) toHash.push_back(hash[i]);
    
    Hash eHash = sha256Hash(toHash.data(), toHash.size());
    BigInt e = bytes32ToBigInt(eHash.data()) % secp256k1::N;
    
    // s = k + e * privateKey (mod N)
    BigInt s = (k + e * privateKey) % secp256k1::N;
    if (s < 0) s = s + secp256k1::N;
    
    // Store R.x in first 32 bytes, s in last 32 bytes
    bigIntToBytes32(R.getX(), result.data());
    bigIntToBytes32(s, result.data() + 32);
    
    return result;
}

Signature signMessage(const std::string& message, const BigInt& privateKey) {
    Hash hash = sha256Hash(message);
    return signHash(hash, privateKey);
}

// Byte array versions of sign functions
Signature signHash(const Hash& hash, const PrivateKey& privateKey) {
    BigInt privKeyBigInt = bytes32ToBigInt(privateKey.data());
    return signHash(hash, privKeyBigInt);
}

Signature signMessage(const std::string& message, const PrivateKey& privateKey) {
    BigInt privKeyBigInt = bytes32ToBigInt(privateKey.data());
    return signMessage(message, privKeyBigInt);
}

bool verifySignature(const Hash& hash, const Signature& signature, const PublicKey& publicKey) {
    // Extract R.x and s from signature
    BigInt Rx = bytes32ToBigInt(signature.data());
    BigInt s = bytes32ToBigInt(signature.data() + 32);
    
    // Decompress public key
    ECPoint P = decompressPublicKey(publicKey);
    if (P.isPointAtInfinity()) {
        return false;
    }
    
    // Compute e = SHA256(R.x || P.x || hash)
    std::vector<uint8_t> toHash;
    uint8_t rxBytes[32], pubxBytes[32];
    bigIntToBytes32(Rx, rxBytes);
    bigIntToBytes32(P.getX(), pubxBytes);
    
    for (int i = 0; i < 32; i++) toHash.push_back(rxBytes[i]);
    for (int i = 0; i < 32; i++) toHash.push_back(pubxBytes[i]);
    for (int i = 0; i < 32; i++) toHash.push_back(hash[i]);
    
    Hash eHash = sha256Hash(toHash.data(), toHash.size());
    BigInt e = bytes32ToBigInt(eHash.data()) % secp256k1::N;
    
    // Verify: s*G == R + e*P
    // Rearranged: R = s*G - e*P
    ECPoint sG = G * s;
    ECPoint eP = P * e;
    
    // Negate eP (negate y coordinate)
    BigInt negY = secp256k1::P - eP.getY();
    ECPoint negEP(eP.getX(), negY, secp256k1::P, secp256k1::A, secp256k1::B);
    
    ECPoint computedR = sG + negEP;
    
    // Check if x-coordinates match
    return computedR.getX() == Rx;
}

bool verifyMessageSignature(const std::string& message, const Signature& signature, const PublicKey& publicKey) {
    Hash hash = sha256Hash(message);
    return verifySignature(hash, signature, publicKey);
}

// =====================================================
// Public Key Compression Implementation
// =====================================================

void bigIntToBytes32(const BigInt& num, uint8_t* output) {
    // Convert BigInt to hex string and pad to 64 hex chars (32 bytes)
    std::string hex;
    BigInt temp = num;
    
    // Convert to hex manually
    const char* hexChars = "0123456789abcdef";
    if (temp == 0) {
        hex = "0";
    } else {
        while (temp > 0) {
            BigInt remainder = temp % 16;
            hex = hexChars[remainder.to_int()] + hex;
            temp = temp / 16;
        }
    }
    
    // Pad to 64 hex characters (32 bytes)
    while (hex.length() < 64) {
        hex = "0" + hex;
    }
    
    // Convert hex string to bytes. The string is built above so it is always
    // well-formed; this just avoids 32 substr allocations per call on a path
    // that runs for every sign and verify.
    for (size_t i = 0; i < 32; i++) {
        output[i] = static_cast<uint8_t>((hexNibble(hex[i * 2]) << 4) | hexNibble(hex[i * 2 + 1]));
    }
}

BigInt bytes32ToBigInt(const uint8_t* data) {
    std::stringstream ss;
    for (size_t i = 0; i < 32; i++) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
    }
    return BigInt(ss.str(), 16);
}

// Modular exponentiation helper for modSqrt
BigInt modPow(const BigInt& base, const BigInt& exp, const BigInt& mod) {
    if (exp == 0) return BigInt(1);
    
    BigInt result = 1;
    BigInt b = base % mod;
    std::string binary = exp.to_binary_string();
    
    for (int i = static_cast<int>(binary.size()) - 1; i >= 0; i--) {
        if (binary[i] == '1') {
            result = (result * b) % mod;
        }
        if (i > 0) {
            b = (b * b) % mod;
        }
    }
    return result;
}

BigInt modSqrt(const BigInt& n, const BigInt& p) {
    // For secp256k1, p ≡ 3 (mod 4), so we can use the simple formula:
    // sqrt(n) = n^((p+1)/4) mod p
    // This is because p = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F
    // which satisfies p ≡ 3 (mod 4)
    
    if (n == 0) return BigInt(0);
    
    // Check if n is a quadratic residue using Euler's criterion
    // n^((p-1)/2) ≡ 1 (mod p) if n is a QR
    BigInt exp = (p - 1) / 2;
    BigInt check = modPow(n, exp, p);
    if (check != 1) {
        return BigInt(0);  // No square root exists
    }
    
    // Since p ≡ 3 (mod 4), use simple formula
    BigInt sqrtExp = (p + 1) / 4;
    return modPow(n, sqrtExp, p);
}

PublicKey compressPublicKey(const ECPoint& point) {
    PublicKey result = {};
    
    if (point.isPointAtInfinity()) {
        return result;  // Return empty/zero key for point at infinity
    }
    
    BigInt x = point.getX();
    BigInt y = point.getY();
    
    // Determine prefix based on whether y is even or odd
    // y is even if y % 2 == 0
    BigInt yMod2 = y % 2;
    
    result[0] = (yMod2 == 0) ? COMPRESSED_EVEN_PREFIX : COMPRESSED_ODD_PREFIX;
    
    // Store x-coordinate in bytes 1-32
    bigIntToBytes32(x, result.data() + 1);
    
    return result;
}

ECPoint decompressPublicKey(const PublicKey& compressedKey) {
    if (compressedKey[0] != COMPRESSED_EVEN_PREFIX && 
        compressedKey[0] != COMPRESSED_ODD_PREFIX) {
        return G.pointAtInfinity();  // Invalid prefix
    }
    
    // Extract x-coordinate
    BigInt x = bytes32ToBigInt(compressedKey.data() + 1);
    
    // Calculate y^2 = x^3 + ax + b (mod p)
    // For secp256k1: y^2 = x^3 + 7 (mod p) since a = 0
    BigInt x_cubed = (x * x * x) % secp256k1::P;
    BigInt y_squared = (x_cubed + secp256k1::B) % secp256k1::P;
    
    // Calculate y using modular square root
    BigInt y = modSqrt(y_squared, secp256k1::P);
    
    if (y == 0 && y_squared != 0) {
        return G.pointAtInfinity();  // No valid y coordinate
    }
    
    // Determine correct y based on prefix (even/odd)
    BigInt yMod2 = y % 2;
    if (yMod2 < 0) yMod2 = yMod2 + 2;
    
    bool yIsEven = (yMod2 == 0);
    bool needEven = (compressedKey[0] == COMPRESSED_EVEN_PREFIX);
    
    if (yIsEven != needEven) {
        y = secp256k1::P - y;  // Use the other root
    }
    
    return ECPoint(x, y, secp256k1::P, secp256k1::A, secp256k1::B);
}

UncompressedPublicKey toUncompressedPublicKey(const ECPoint& point) {
    UncompressedPublicKey result = {};
    
    if (point.isPointAtInfinity()) {
        return result;
    }
    
    result[0] = UNCOMPRESSED_PREFIX;
    bigIntToBytes32(point.getX(), result.data() + 1);
    bigIntToBytes32(point.getY(), result.data() + 33);
    
    return result;
}

ECPoint fromUncompressedPublicKey(const UncompressedPublicKey& uncompressedKey) {
    if (uncompressedKey[0] != UNCOMPRESSED_PREFIX) {
        return G.pointAtInfinity();  // Invalid prefix
    }
    
    BigInt x = bytes32ToBigInt(uncompressedKey.data() + 1);
    BigInt y = bytes32ToBigInt(uncompressedKey.data() + 33);
    
    return ECPoint(x, y, secp256k1::P, secp256k1::A, secp256k1::B);
}

PublicKey compressPublicKey(const UncompressedPublicKey& uncompressedKey) {
    ECPoint point = fromUncompressedPublicKey(uncompressedKey);
    return compressPublicKey(point);
}

UncompressedPublicKey decompressToUncompressed(const PublicKey& compressedKey) {
    ECPoint point = decompressPublicKey(compressedKey);
    return toUncompressedPublicKey(point);
}

bool isCompressedPublicKey(const uint8_t* data, size_t size) {
    if (size != PUBLIC_KEY_SIZE) return false;
    return (data[0] == COMPRESSED_EVEN_PREFIX || data[0] == COMPRESSED_ODD_PREFIX);
}

bool isUncompressedPublicKey(const uint8_t* data, size_t size) {
    if (size != UNCOMPRESSED_PUBLIC_KEY_SIZE) return false;
    return (data[0] == UNCOMPRESSED_PREFIX);
}

}  // namespace ECCrypto
