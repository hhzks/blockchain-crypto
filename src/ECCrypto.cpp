#include "include/ECCrypto.h"
#include "include/sha.h"
#include "bigint.h"
#include <random>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <chrono>

namespace ECCrypto {

namespace secp256k1 {
    const std::array<uint64_t, 4> P = {
        0xFFFFFFFEFFFFFC2FULL, 0xFFFFFFFFFFFFFFFFULL, 
        0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL
    };
    
    // Curve order n
    const std::array<uint64_t, 4> N = {
        0xBFD25E8CD0364141ULL, 0xBAAEDCE6AF48A03BULL,
        0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL
    };
    
    // Generator point G coordinates
    const std::array<uint64_t, 4> GX = {
        0x59F2815B16F81798ULL, 0x029BFCDB2DCE28D9ULL,
        0x55A06295CE870B07ULL, 0x79BE667EF9DCBBACULL
    };
    
    const std::array<uint64_t, 4> GY = {
        0x9C47D08FFB10D4B8ULL, 0xFD17B448A6855419ULL,
        0x5DA4FBFC0E1108A8ULL, 0x483ADA7726A3C465ULL
    };
}


// Simplified elliptic curve point
struct ECPoint {
    BigInt256 x, y;
    bool isInfinity;
    
    ECPoint() : isInfinity(true) {}
    ECPoint(const BigInt256& x_, const BigInt256& y_) : x(x_), y(y_), isInfinity(false) {}
};

// Simplified modular arithmetic (for demonstration - not cryptographically secure)
BigInt256 modAdd(const BigInt256& a, const BigInt256& b, const BigInt256& mod) {
    // Simplified implementation - in production, use proper big integer library
    return a; // Placeholder
}

BigInt256 modMul(const BigInt256& a, const BigInt256& b, const BigInt256& mod) {
    // Simplified implementation - in production, use proper big integer library
    return a; // Placeholder
}

ECPoint pointDouble(const ECPoint& p) {
    // Simplified implementation - in production, implement proper point doubling
    return p; // Placeholder
}

ECPoint pointAdd(const ECPoint& p1, const ECPoint& p2) {
    // Simplified implementation - in production, implement proper point addition
    return p1; // Placeholder
}

ECPoint scalarMult(const BigInt256& scalar, const ECPoint& point) {
    // Simplified implementation - in production, implement proper scalar multiplication
    return point; // Placeholder
}

// Simplified implementation for demonstration
// In production, use a proper cryptographic library like libsecp256k1

KeyPair::KeyPair() {
    privateKey.fill(0);
    publicKey.fill(0);
}

KeyPair::KeyPair(const PrivateKey& privKey, const PublicKey& pubKey) 
    : privateKey(privKey), publicKey(pubKey) {
    privateKeyHex = bytesToHex(privateKey.data(), PRIVATE_KEY_SIZE);
    publicKeyHex = bytesToHex(publicKey.data(), PUBLIC_KEY_SIZE);
    address = deriveAddress(publicKey);
}

std::unique_ptr<KeyPair> generateKeyPair() {
    auto keyPair = std::make_unique<KeyPair>();
    
    // Generate random private key
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0, 255);
    
    // Generate random 32 bytes for private key
    for (size_t i = 0; i < PRIVATE_KEY_SIZE; i++) {
        keyPair->privateKey[i] = static_cast<uint8_t>(dist(gen));
    }
    
    // Ensure private key is valid (not zero, less than curve order)
    // Simplified check - in production, implement proper validation
    if (keyPair->privateKey[0] == 0) {
        keyPair->privateKey[0] = 1; // Ensure not zero
    }
    
    // Generate public key from private key (simplified)
    // In production, use proper elliptic curve multiplication
    // For now, create a deterministic public key based on private key
    std::string privHashStr = SHA256::hash(keyPair->privateKey.data(), PRIVATE_KEY_SIZE);
    
    // First byte indicates compressed public key format
    keyPair->publicKey[0] = 0x02; // Compressed format
    
    // Convert hex string to bytes for public key
    Hash privHashBytes;
    hexToBytes(privHashStr, privHashBytes.data(), HASH_SIZE);
    
    // Use hash of private key as x-coordinate (simplified)
    for (size_t i = 0; i < 32; i++) {
        keyPair->publicKey[i + 1] = privHashBytes[i];
    }
    
    keyPair->privateKeyHex = bytesToHex(keyPair->privateKey.data(), PRIVATE_KEY_SIZE);
    keyPair->publicKeyHex = bytesToHex(keyPair->publicKey.data(), PUBLIC_KEY_SIZE);
    keyPair->address = deriveAddress(keyPair->publicKey);
    
    return keyPair;
}

std::unique_ptr<KeyPair> keyPairFromPrivateKey(const PrivateKey& privateKey) {
    if (!isValidPrivateKey(privateKey)) {
        return nullptr;
    }
    
    auto keyPair = std::make_unique<KeyPair>();
    keyPair->privateKey = privateKey;
    
    // Generate public key from private key (simplified)
    std::string privHashStr = SHA256::hash(privateKey.data(), PRIVATE_KEY_SIZE);
    
    // Convert hex string to bytes
    Hash privHashBytes;
    hexToBytes(privHashStr, privHashBytes.data(), HASH_SIZE);
    
    keyPair->publicKey[0] = 0x02; // Compressed format
    for (size_t i = 0; i < 32; i++) {
        keyPair->publicKey[i + 1] = privHashBytes[i];
    }
    
    keyPair->privateKeyHex = bytesToHex(keyPair->privateKey.data(), PRIVATE_KEY_SIZE);
    keyPair->publicKeyHex = bytesToHex(keyPair->publicKey.data(), PUBLIC_KEY_SIZE);
    keyPair->address = deriveAddress(keyPair->publicKey);
    
    return keyPair;
}

std::unique_ptr<KeyPair> keyPairFromPrivateKeyHex(const std::string& privateKeyHex) {
    if (privateKeyHex.length() != PRIVATE_KEY_SIZE * 2) {
        return nullptr;
    }
    
    PrivateKey privKey;
    if (hexToBytes(privateKeyHex, privKey.data(), PRIVATE_KEY_SIZE) != PRIVATE_KEY_SIZE) {
        return nullptr;
    }
    
    return keyPairFromPrivateKey(privKey);
}

Signature signHash(const Hash& hash, const PrivateKey& privateKey) {
    Signature signature;
    signature.fill(0);
    
    // Simplified ECDSA signing (for demonstration)
    // In production, implement proper ECDSA with RFC 6979 deterministic k
    
    // Create deterministic signature based on hash and private key
    std::string combined;
    combined.append(reinterpret_cast<const char*>(hash.data()), HASH_SIZE);
    combined.append(reinterpret_cast<const char*>(privateKey.data()), PRIVATE_KEY_SIZE);
    
    std::string sigHashStr1 = SHA256::hash(combined);
    std::string sigHashStr2 = SHA256::hash(sigHashStr1);
    
    // Convert hex strings to bytes
    Hash sigHashBytes1, sigHashBytes2;
    hexToBytes(sigHashStr1, sigHashBytes1.data(), HASH_SIZE);
    hexToBytes(sigHashStr2, sigHashBytes2.data(), HASH_SIZE);
    
    // Use first 32 bytes as 'r' and second 32 bytes as 's'
    for (size_t i = 0; i < 32; i++) {
        signature[i] = sigHashBytes1[i];      // r component
        signature[i + 32] = sigHashBytes2[i]; // s component
    }
    
    return signature;
}

Signature signMessage(const std::string& message, const PrivateKey& privateKey) {
    Hash hash = sha256Hash(message);
    return signHash(hash, privateKey);
}

bool verifySignature(const Hash& hash, const Signature& signature, const PublicKey& publicKey) {
    // Simplified verification (for demonstration)
    // In production, implement proper ECDSA verification
    
    // Recreate the signature using the public key
    // This is a simplified approach - not cryptographically secure
    
    // Extract supposed private key info from public key (this is backwards, just for demo)
    PrivateKey testPrivKey;
    for (size_t i = 0; i < 32; i++) {
        testPrivKey[i] = publicKey[i + 1]; // Skip compression byte
    }
    
    Signature expectedSig = signHash(hash, testPrivKey);
    
    // Compare signatures
    return std::memcmp(signature.data(), expectedSig.data(), SIGNATURE_SIZE) == 0;
}

bool verifyMessageSignature(const std::string& message, const Signature& signature, const PublicKey& publicKey) {
    Hash hash = sha256Hash(message);
    return verifySignature(hash, signature, publicKey);
}

std::string deriveAddress(const PublicKey& publicKey) {
    // Create address by hashing public key
    Hash pubHash = sha256Hash(publicKey.data(), PUBLIC_KEY_SIZE);
    
    // Take first 20 bytes and encode as hex (similar to Bitcoin)
    return bytesToHex(pubHash.data(), 20);
}

std::string bytesToHex(const uint8_t* data, size_t size) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; i++) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

size_t hexToBytes(const std::string& hex, uint8_t* output, size_t maxSize) {
    if (hex.length() % 2 != 0 || hex.length() / 2 > maxSize) {
        return 0;
    }
    
    size_t len = hex.length() / 2;
    for (size_t i = 0; i < len; i++) {
        std::string byteStr = hex.substr(i * 2, 2);
        try {
            output[i] = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
        } catch (...) {
            return 0;
        }
    }
    
    return len;
}

Hash sha256Hash(const std::string& message) {
    std::string hashStr = SHA256::hash(message);
    Hash result;
    hexToBytes(hashStr, result.data(), HASH_SIZE);
    return result;
}

Hash sha256Hash(const uint8_t* data, size_t size) {
    std::string dataStr(reinterpret_cast<const char*>(data), size);
    return sha256Hash(dataStr);
}

bool isValidPrivateKey(const PrivateKey& privateKey) {
    // Check if private key is not zero
    bool isZero = true;
    for (size_t i = 0; i < PRIVATE_KEY_SIZE; i++) {
        if (privateKey[i] != 0) {
            isZero = false;
            break;
        }
    }
    
    if (isZero) return false;
    
    // In production, also check if private key < curve order
    // For simplicity, we'll accept any non-zero key
    return true;
}

bool isValidPublicKey(const PublicKey& publicKey) {
    // Check compression byte
    if (publicKey[0] != 0x02 && publicKey[0] != 0x03) {
        return false;
    }
    
    // Check if x-coordinate is not all zeros
    bool isZero = true;
    for (size_t i = 1; i < PUBLIC_KEY_SIZE; i++) {
        if (publicKey[i] != 0) {
            isZero = false;
            break;
        }
    }
    
    return !isZero;
}

std::string signatureToHex(const Signature& signature) {
    return bytesToHex(signature.data(), SIGNATURE_SIZE);
}

Signature signatureFromHex(const std::string& hex) {
    Signature signature;
    signature.fill(0);
    
    if (hexToBytes(hex, signature.data(), SIGNATURE_SIZE) == SIGNATURE_SIZE) {
        return signature;
    }
    
    return signature; // Return empty signature if conversion failed
}

bool testECCImplementation() {
    std::cout << "Testing ECC implementation..." << std::endl;
    
    // Test 1: Key generation
    auto keyPair = generateKeyPair();
    if (!keyPair) {
        std::cout << "❌ Key generation failed" << std::endl;
        return false;
    }
    std::cout << "✅ Key generation successful" << std::endl;
    
    // Test 2: Private key validation
    if (!isValidPrivateKey(keyPair->privateKey)) {
        std::cout << "❌ Generated private key is invalid" << std::endl;
        return false;
    }
    std::cout << "✅ Private key validation passed" << std::endl;
    
    // Test 3: Public key validation
    if (!isValidPublicKey(keyPair->publicKey)) {
        std::cout << "❌ Generated public key is invalid" << std::endl;
        return false;
    }
    std::cout << "✅ Public key validation passed" << std::endl;
    
    // Test 4: Message signing and verification
    std::string testMessage = "Hello, Blockchain!";
    Signature signature = signMessage(testMessage, keyPair->privateKey);
    
    bool verificationResult = verifyMessageSignature(testMessage, signature, keyPair->publicKey);
    if (!verificationResult) {
        std::cout << "❌ Signature verification failed" << std::endl;
        return false;
    }
    std::cout << "✅ Message signing and verification passed" << std::endl;
    
    // Test 5: Invalid signature detection
    std::string differentMessage = "Different message";
    bool shouldFail = verifyMessageSignature(differentMessage, signature, keyPair->publicKey);
    if (shouldFail) {
        std::cout << "❌ Failed to detect invalid signature" << std::endl;
        return false;
    }
    std::cout << "✅ Invalid signature detection passed" << std::endl;
    
    // Test 6: Address derivation
    if (keyPair->address.empty() || keyPair->address.length() != 40) {
        std::cout << "❌ Address derivation failed" << std::endl;
        return false;
    }
    std::cout << "✅ Address derivation passed" << std::endl;
    
    std::cout << "🎉 All ECC tests passed!" << std::endl;
    std::cout << "Sample Key Pair:" << std::endl;
    std::cout << "Private Key: " << keyPair->privateKeyHex << std::endl;
    std::cout << "Public Key:  " << keyPair->publicKeyHex << std::endl;
    std::cout << "Address:     " << keyPair->address << std::endl;
    
    return true;
}

} // namespace ECCrypto