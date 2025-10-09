#include "include/Wallet.h"
#include "include/Transaction.h"
#include "include/utils.h"
#include "include/sha.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iomanip>

namespace wallet {

std::string Wallet::generateNewAddress() {
    try {
        // Generate new key pair
        auto keyPair = ECCrypto::generateKeyPair();
        
        // Store the key pair
        auto keyPairStruct = std::make_unique<KeyPair>();
        keyPairStruct->privateKey = keyPair->privateKey;
        keyPairStruct->publicKey = keyPair->publicKey;
        keyPairStruct->address = keyPair->address;
        
        keyPairs[keyPair->address] = std::move(keyPairStruct);
        
        // Set as default if it's the first address
        if (defaultAddress.empty()) {
            defaultAddress = keyPair->address;
        }
        
        std::cout << "Generated new address: " << keyPair->address << std::endl;
        return keyPair->address;
        
    } catch (const std::exception& e) {
        std::cerr << "Error generating new address: " << e.what() << std::endl;
        return "";
    }
}

bool Wallet::importKeyPair(const std::string& privateKeyHex, const std::string& publicKeyHex) {
    if (privateKeyHex.length() != ECCrypto::PRIVATE_KEY_SIZE * 2 ||
        publicKeyHex.length() != ECCrypto::PUBLIC_KEY_SIZE * 2) {
        std::cerr << "Invalid key lengths" << std::endl;
        return false;
    }
    
    ECCrypto::PrivateKey privateKey;
    ECCrypto::PublicKey publicKey;
    
    if (ECCrypto::hexToBytes(privateKeyHex, privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE ||
        ECCrypto::hexToBytes(publicKeyHex, publicKey.data(), ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
        std::cerr << "Failed to parse key hex strings" << std::endl;
        return false;
    }
    
    if (!ECCrypto::isValidPrivateKey(privateKey) || !ECCrypto::isValidPublicKey(publicKey)) {
        std::cerr << "Invalid keys provided" << std::endl;
        return false;
    }
    
    // Derive address
    std::string address = ECCrypto::deriveAddress(publicKey);
    
    // Store the key pair
    auto keyPair = std::make_unique<KeyPair>();
    keyPair->privateKey = privateKey;
    keyPair->publicKey = publicKey;
    keyPair->address = address;
    
    keyPairs[address] = std::move(keyPair);
    
    if (defaultAddress.empty()) {
        defaultAddress = address;
    }
    
    std::cout << "Imported key pair for address: " << address << std::endl;
    return true;
}

bool Wallet::importPrivateKey(const std::string& privateKeyHex) {
    if (privateKeyHex.length() != ECCrypto::PRIVATE_KEY_SIZE * 2) {
        std::cerr << "Invalid private key length" << std::endl;
        return false;
    }
    
    ECCrypto::PrivateKey privateKey;
    if (ECCrypto::hexToBytes(privateKeyHex, privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE) {
        std::cerr << "Failed to parse private key hex" << std::endl;
        return false;
    }
    
    if (!ECCrypto::isValidPrivateKey(privateKey)) {
        std::cerr << "Invalid private key" << std::endl;
        return false;
    }
    
    try {
        // Use ECCrypto to get key pair from private key
        auto keyPair = ECCrypto::keyPairFromPrivateKey(privateKey);
        if (!keyPair) {
            std::cerr << "Failed to derive public key from private key" << std::endl;
            return false;
        }
        
        // Store the key pair
        auto keyPairStruct = std::make_unique<KeyPair>();
        keyPairStruct->privateKey = keyPair->privateKey;
        keyPairStruct->publicKey = keyPair->publicKey;
        keyPairStruct->address = keyPair->address;
        
        keyPairs[keyPair->address] = std::move(keyPairStruct);
        
        if (defaultAddress.empty()) {
            defaultAddress = keyPair->address;
        }
        
        std::cout << "Imported private key for address: " << keyPair->address << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error deriving public key: " << e.what() << std::endl;
        return false;
    }
}

ECCrypto::PrivateKey Wallet::getPrivateKey(const std::string& address) const {
    auto it = keyPairs.find(address);
    if (it != keyPairs.end()) {
        return it->second->privateKey;
    }
    throw std::runtime_error("Address not found in wallet: " + address);
}

ECCrypto::PublicKey Wallet::getPublicKey(const std::string& address) const {
    auto it = keyPairs.find(address);
    if (it != keyPairs.end()) {
        return it->second->publicKey;
    }
    throw std::runtime_error("Address not found in wallet: " + address);
}

std::string Wallet::getPrivateKeyHex(const std::string& address) const {
    try {
        ECCrypto::PrivateKey privateKey = getPrivateKey(address);
        return ECCrypto::bytesToHex(privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE);
    } catch (const std::exception& e) {
        return "";
    }
}

std::string Wallet::getPublicKeyHex(const std::string& address) const {
    try {
        ECCrypto::PublicKey publicKey = getPublicKey(address);
        return ECCrypto::bytesToHex(publicKey.data(), ECCrypto::PUBLIC_KEY_SIZE);
    } catch (const std::exception& e) {
        return "";
    }
}

bool Wallet::hasAddress(const std::string& address) const {
    return keyPairs.find(address) != keyPairs.end();
}

std::vector<std::string> Wallet::getAllAddresses() const {
    std::vector<std::string> addresses;
    addresses.reserve(keyPairs.size());
    
    for (const auto& pair : keyPairs) {
        addresses.push_back(pair.first);
    }
    
    return addresses;
}

void Wallet::setDefaultAddress(const std::string& address) {
    if (hasAddress(address)) {
        defaultAddress = address;
    } else {
        std::cerr << "Cannot set default address: Address not found in wallet" << std::endl;
    }
}

std::string Wallet::getDefaultAddress() const {
    return defaultAddress;
}

bool Wallet::signTransaction(Transaction& transaction, const std::string& fromAddress) const {
    if (!hasAddress(fromAddress)) {
        std::cerr << "Address not found in wallet: " << fromAddress << std::endl;
        return false;
    }
    
    try {
        ECCrypto::PrivateKey privateKey = getPrivateKey(fromAddress);
        return transaction.signTransaction(privateKey);
    } catch (const std::exception& e) {
        std::cerr << "Error signing transaction: " << e.what() << std::endl;
        return false;
    }
}

bool Wallet::verifyTransaction(const Transaction& transaction, const std::string& address) const {
    if (!hasAddress(address)) {
        std::cerr << "Address not found in wallet: " << address << std::endl;
        return false;
    }
    
    try {
        ECCrypto::PublicKey publicKey = getPublicKey(address);
        return transaction.verifySignature(publicKey);
    } catch (const std::exception& e) {
        std::cerr << "Error verifying transaction: " << e.what() << std::endl;
        return false;
    }
}

bool Wallet::saveToFile(const std::string& filename, const std::string& password) const {
    // This is a simplified implementation - in production, use proper encryption
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open wallet file for writing: " << filename << std::endl;
        return false;
    }
    
    // Simple XOR encryption with password (NOT secure for production use)
    auto encrypt = [&password](const std::string& data) -> std::string {
        std::string encrypted = data;
        for (size_t i = 0; i < encrypted.length(); ++i) {
            encrypted[i] ^= password[i % password.length()];
        }
        return encrypted;
    };
    
    try {
        file << "WALLET_FILE_V1" << std::endl;
        file << defaultAddress << std::endl;
        file << keyPairs.size() << std::endl;
        
        for (const auto& pair : keyPairs) {
            const std::string& address = pair.first;
            const auto& keyPair = pair.second;
            
            std::string privateKeyHex = ECCrypto::bytesToHex(keyPair->privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE);
            std::string publicKeyHex = ECCrypto::bytesToHex(keyPair->publicKey.data(), ECCrypto::PUBLIC_KEY_SIZE);
            
            file << encrypt(address) << std::endl;
            file << encrypt(privateKeyHex) << std::endl;
            file << encrypt(publicKeyHex) << std::endl;
        }
        
        std::cout << "Wallet saved to file: " << filename << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving wallet: " << e.what() << std::endl;
        return false;
    }
}

bool Wallet::loadFromFile(const std::string& filename, const std::string& password) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open wallet file for reading: " << filename << std::endl;
        return false;
    }
    
    // Simple XOR decryption with password
    auto decrypt = [&password](const std::string& data) -> std::string {
        std::string decrypted = data;
        for (size_t i = 0; i < decrypted.length(); ++i) {
            decrypted[i] ^= password[i % password.length()];
        }
        return decrypted;
    };
    
    try {
        std::string header;
        std::getline(file, header);
        if (header != "WALLET_FILE_V1") {
            std::cerr << "Invalid wallet file format" << std::endl;
            return false;
        }
        
        clear(); // Clear existing keys
        
        std::getline(file, defaultAddress);
        
        size_t numKeyPairs;
        file >> numKeyPairs;
        file.ignore(); // ignore newline after number
        
        for (size_t i = 0; i < numKeyPairs; ++i) {
            std::string encryptedAddress, encryptedPrivateKey, encryptedPublicKey;
            std::getline(file, encryptedAddress);
            std::getline(file, encryptedPrivateKey);
            std::getline(file, encryptedPublicKey);
            
            std::string address = decrypt(encryptedAddress);
            std::string privateKeyHex = decrypt(encryptedPrivateKey);
            std::string publicKeyHex = decrypt(encryptedPublicKey);
            
            if (!importKeyPair(privateKeyHex, publicKeyHex)) {
                std::cerr << "Failed to import key pair from wallet file" << std::endl;
                clear();
                return false;
            }
        }
        
        std::cout << "Wallet loaded from file: " << filename << std::endl;
        std::cout << "Loaded " << keyPairs.size() << " key pairs" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading wallet: " << e.what() << std::endl;
        clear();
        return false;
    }
}

void Wallet::clear() {
    keyPairs.clear();
    defaultAddress.clear();
}

std::string Wallet::toString() const {
    std::stringstream ss;
    ss << "Wallet Summary:" << std::endl;
    ss << "  Total addresses: " << keyPairs.size() << std::endl;
    ss << "  Default address: " << (defaultAddress.empty() ? "None" : defaultAddress) << std::endl;
    ss << "  Addresses:" << std::endl;
    
    for (const auto& pair : keyPairs) {
        const std::string& address = pair.first;
        ss << "    " << address;
        if (address == defaultAddress) {
            ss << " (default)";
        }
        ss << std::endl;
    }
    
    return ss.str();
}

namespace utils {

std::unique_ptr<Wallet> createRandomWallet() {
    auto wallet = std::make_unique<Wallet>();
    wallet->generateNewAddress();
    return wallet;
}

bool isValidAddress(const std::string& address) {
    // Simple validation - check if it looks like a hex string of expected length
    if (address.length() != 40) { // 20 bytes = 40 hex chars
        return false;
    }
    
    for (char c : address) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    
    return true;
}

std::string generateMnemonic() {
    // Simplified mnemonic generation using a word list
    const std::vector<std::string> wordList = {
        "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract",
        "absurd", "abuse", "access", "accident", "account", "accuse", "achieve", "acid",
        "across", "act", "action", "actor", "actress", "actual", "adapt", "add",
        "address", "adjust", "admit", "adult", "advance", "advice", "aerobic", "affair"
    };
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, wordList.size() - 1);
    
    std::stringstream ss;
    for (int i = 0; i < 12; ++i) {
        if (i > 0) ss << " ";
        ss << wordList[dis(gen)];
    }
    
    return ss.str();
}

std::pair<ECCrypto::PrivateKey, ECCrypto::PublicKey> recoverFromMnemonic(const std::string& mnemonic) {
    // Simplified recovery - hash the mnemonic to create a seed
    std::string seed = SHA256::hash(mnemonic);
    
    // Use first 32 bytes of hash as private key and generate key pair
    ECCrypto::PrivateKey privateKey;
    std::copy(seed.begin(), seed.begin() + std::min(seed.length(), size_t(ECCrypto::PRIVATE_KEY_SIZE)), privateKey.begin());
    
    auto keyPair = ECCrypto::keyPairFromPrivateKey(privateKey);
    if (!keyPair) {
        throw std::runtime_error("Failed to create key pair from mnemonic");
    }
    
    return {keyPair->privateKey, keyPair->publicKey};
}

} // namespace utils

} // namespace wallet