#pragma once

#include "bigint.h"
#include "sha.h"
#include <string>
#include <vector>
#include <memory>
#include <array>

namespace ECCrypto {

    namespace secp256k1 {
    //curve parameters
    const BigInt P ("115792089237316195423570985008687907853269984665640564039457584007908834671663");
    const BigInt A (0);
    const BigInt B (7);

    //generator point stuff
    const BigInt GX ("55066263022277343669578718895168534326250603453777594175500187360389116729240");
    const BigInt GY ("32670510020758816978083085130507043184471273380659243275938904335757337482424");
    const BigInt N ("115792089237316195423570985008687907852837564279074904382605163141518161494337");

    };
    
    constexpr size_t PRIVATE_KEY_SIZE = 32;
    constexpr size_t PUBLIC_KEY_SIZE = 33;  // Compressed public key
    constexpr size_t SIGNATURE_SIZE = 64;   // r + s values
    constexpr size_t HASH_SIZE = 32;        // SHA-256 hash size
    
    using Hash = std::array<uint8_t, HASH_SIZE>;
    using Signature = std::array<uint8_t, SIGNATURE_SIZE>;
    using PublicKey = std::array<uint8_t, PUBLIC_KEY_SIZE>;
    using PrivateKey = std::array<uint8_t, PRIVATE_KEY_SIZE>;

    class ECPoint{
        private:
            BigInt x, y, p, a, b;
            bool isInfinity;
        public:
            ECPoint(BigInt GX, BigInt GY, BigInt P, BigInt A, BigInt B) : x(GX), y(GY), p(P), a(A), b(B) {isInfinity = false;}
            ECPoint& operator=(const ECPoint& other) = default;

            BigInt getX() const;
            BigInt getY() const;

            ECPoint pointAtInfinity() const;
            ECPoint doubledPoint() const;
            ECPoint operator+(const ECPoint& other) const;
            ECPoint operator*(const BigInt& scalar) const;
            ECPoint operator+=(const ECPoint& other);
            ECPoint operator*=(const BigInt& scalar);

    };

    inline const ECPoint G{secp256k1::GX, secp256k1::GY, secp256k1::P, secp256k1::A, secp256k1::B};
    
    /**
     * Key pair structure containing both private and public keys
     */
    struct KeyPair {
        BigInt privateKey;
        ECPoint publicKey;
        std::string privateKeyHex;
        std::string publicKeyHex;
        std::string address;  // Derived address for blockchain use
        
        KeyPair();
        KeyPair(const BigInt& private_key, const ECPoint& public_key);
    };
    
    /**
     * Generate a new random key pair using secp256k1
     * @return Unique pointer to new KeyPair
     */
    std::unique_ptr<KeyPair> generateKeyPair();
    
    /**
     * Create KeyPair from existing private key
     * @param privateKey The private key bytes
     * @return Unique pointer to KeyPair or nullptr if invalid
     */
    std::unique_ptr<KeyPair> keyPairFromPrivateKey(const BigInt& privateKey);
    
    /**
     * Create KeyPair from hex-encoded private key string
     * @param privateKeyHex Hex string of private key
     * @return Unique pointer to KeyPair or nullptr if invalid
     */
    std::unique_ptr<KeyPair> keyPairFromPrivateKeyHex(const std::string& privateKeyHex);
    
    /**
     * Sign a hash using Schnorr with secp256k1
     * @param hash The 32-byte hash to sign
     * @param privateKey The private key to sign with
     * @return Signature bytes, or empty array if signing failed
     */
    Signature signHash(const Hash& hash, const BigInt& privateKey);
    
    /**
     * Sign a message string (will be hashed with SHA-256 first)
     * @param message The message to sign
     * @param privateKey The private key to sign with
     * @return Signature bytes, or empty array if signing failed
     */
    Signature signMessage(const std::string& message, const BigInt& privateKey);
    
    /**
     * Verify a Schnorr signature
     * @param hash The original hash that was signed
     * @param signature The signature to verify
     * @param publicKey The public key to verify against
     * @return True if signature is valid, false otherwise
     */
    bool verifySignature(const Hash& hash, const Signature& signature, const PublicKey& publicKey);
    
    /**
     * Verify a signature for a message string
     * @param message The original message
     * @param signature The signature to verify
     * @param publicKey The public key to verify against
     * @return True if signature is valid, false otherwise
     */
    bool verifyMessageSignature(const std::string& message, const Signature& signature, const PublicKey& publicKey);
    
    /**
     * Derive a blockchain address from a public key
     * @param publicKey The public key
     * @return Address string (hex-encoded hash of public key)
     */
    std::string deriveAddress(const PublicKey& publicKey);
    
    /**
     * Convert byte array to hex string
     * @param data The byte data
     * @param size The size of the data
     * @return Hex string representation
     */
    std::string bytesToHex(const uint8_t* data, size_t size);
    
    /**
     * Convert hex string to byte array
     * @param hex The hex string
     * @param output Output buffer
     * @param maxSize Maximum size of output buffer
     * @return Number of bytes written, or 0 if error
     */
    size_t hexToBytes(const std::string& hex, uint8_t* output, size_t maxSize);
    
    /**
     * Hash a message using SHA-256
     * @param message The message to hash
     * @return 32-byte hash
     */
    Hash sha256Hash(const std::string& message);
    
    /**
     * Hash data using SHA-256
     * @param data Pointer to data
     * @param size Size of data
     * @return 32-byte hash
     */
    Hash sha256Hash(const uint8_t* data, size_t size);
    
    /**
     * Validate that a private key is valid for secp256k1
     * @param privateKey The private key to validate
     * @return True if valid, false otherwise
     */
    bool isValidPrivateKey(const PrivateKey& privateKey);
    
    /**
     * Validate that a public key is valid for secp256k1
     * @param publicKey The public key to validate
     * @return True if valid, false otherwise
     */
    bool isValidPublicKey(const PublicKey& publicKey);
    
    /**
     * Convert signature to hex string representation
     * @param signature The signature
     * @return Hex string
     */
    std::string signatureToHex(const Signature& signature);
    
    /**
     * Convert hex string to signature
     * @param hex The hex string
     * @return Signature, or empty array if invalid
     */
    Signature signatureFromHex(const std::string& hex);
    
    /**
     * Test the ECC implementation with known test vectors
     * @return True if all tests pass, false otherwise
     */
    bool testECCImplementation();
}