#include "include/Wallet.h"
#include "include/Transaction.h"
#include "include/utils.h"
#include "include/sha.h"
#include "include/ECCrypto.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iomanip>

namespace wallet {

std::string Wallet::generateNewAddress() {
    try {
        // Generate a new ECC key pair using ECCrypto
        auto eccKeyPair = ECCrypto::generateKeyPair();
        if (!eccKeyPair) {
            std::cerr << "Failed to generate ECC key pair" << std::endl;
            return "";
        }
        
        // Create our internal KeyPair structure
        auto keyPair = std::make_unique<KeyPair>();
        
        // Convert private key hex to bytes
        if (ECCrypto::hexToBytes(eccKeyPair->privateKeyHex, keyPair->privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE) {
            std::cerr << "Failed to convert private key to bytes" << std::endl;
            return "";
        }
        
        // Convert public key hex to bytes
        if (ECCrypto::hexToBytes(eccKeyPair->publicKeyHex, keyPair->publicKey.data(), ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
            std::cerr << "Failed to convert public key to bytes" << std::endl;
            return "";
        }
        
        keyPair->address = eccKeyPair->address;
        
        // Store the key pair
        std::string address = keyPair->address;
        keyPairs[address] = std::move(keyPair);
        
        // Set as default if this is the first address
        if (defaultAddress.empty()) {
            defaultAddress = address;
        }
        
        return address;
    }
    catch (const std::exception& e) {
        std::cerr << "Error generating new address: " << e.what() << std::endl;
        return "";
    }
}

bool Wallet::importKeyPair(const std::string& privateKeyHex, const std::string& publicKeyHex) {
    try {
        // Validate hex strings
        if (privateKeyHex.length() != ECCrypto::PRIVATE_KEY_SIZE * 2 || 
            publicKeyHex.length() != ECCrypto::PUBLIC_KEY_SIZE * 2) {
            std::cerr << "Invalid key length" << std::endl;
            return false;
        }
        
        // Convert hex strings to byte arrays
        ECCrypto::PrivateKey privateKey;
        ECCrypto::PublicKey publicKey;
        
        if (ECCrypto::hexToBytes(privateKeyHex, privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE ||
            ECCrypto::hexToBytes(publicKeyHex, publicKey.data(), ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
            std::cerr << "Failed to convert hex keys to bytes" << std::endl;
            return false;
        }
        
        // Validate keys
        if (!ECCrypto::isValidPrivateKey(privateKey) || !ECCrypto::isValidPublicKey(publicKey)) {
            std::cerr << "Invalid key pair" << std::endl;
            return false;
        }
        
        // Derive address from public key
        std::string address = ECCrypto::deriveAddress(publicKey);
        
        // Create and store key pair
        auto keyPair = std::make_unique<KeyPair>();
        keyPair->privateKey = privateKey;
        keyPair->publicKey = publicKey;
        keyPair->address = address;
        
        keyPairs[address] = std::move(keyPair);
        
        // Set as default if this is the first address
        if (defaultAddress.empty()) {
            defaultAddress = address;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error importing key pair: " << e.what() << std::endl;
        return false;
    }
}

bool Wallet::importPrivateKey(const std::string& privateKeyHex) {
    try {
        // Generate key pair from private key
        auto eccKeyPair = ECCrypto::keyPairFromPrivateKeyHex(privateKeyHex);
        if (!eccKeyPair) {
            std::cerr << "Failed to generate key pair from private key" << std::endl;
            return false;
        }
        
        // Create our internal KeyPair structure
        auto keyPair = std::make_unique<KeyPair>();
        
        // Convert private key hex to bytes
        if (ECCrypto::hexToBytes(eccKeyPair->privateKeyHex, keyPair->privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE) {
            std::cerr << "Failed to convert private key to bytes" << std::endl;
            return false;
        }
        
        // Convert public key hex to bytes
        if (ECCrypto::hexToBytes(eccKeyPair->publicKeyHex, keyPair->publicKey.data(), ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
            std::cerr << "Failed to convert public key to bytes" << std::endl;
            return false;
        }
        
        keyPair->address = eccKeyPair->address;
        
        // Store the key pair
        std::string address = keyPair->address;
        keyPairs[address] = std::move(keyPair);
        
        // Set as default if this is the first address
        if (defaultAddress.empty()) {
            defaultAddress = address;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error importing private key: " << e.what() << std::endl;
        return false;
    }
}

ECCrypto::PrivateKey Wallet::getPrivateKey(const std::string& address) const {
    auto it = keyPairs.find(address);
    if (it != keyPairs.end()) {
        return it->second->privateKey;
    }
    return ECCrypto::PrivateKey{}; // Return empty array if not found
}

ECCrypto::PublicKey Wallet::getPublicKey(const std::string& address) const {
    auto it = keyPairs.find(address);
    if (it != keyPairs.end()) {
        return it->second->publicKey;
    }
    return ECCrypto::PublicKey{}; // Return empty array if not found
}

std::string Wallet::getPrivateKeyHex(const std::string& address) const {
    auto it = keyPairs.find(address);
    if (it != keyPairs.end()) {
        return ECCrypto::bytesToHex(it->second->privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE);
    }
    return "";
}

std::string Wallet::getPublicKeyHex(const std::string& address) const {
    auto it = keyPairs.find(address);
    if (it != keyPairs.end()) {
        return ECCrypto::bytesToHex(it->second->publicKey.data(), ECCrypto::PUBLIC_KEY_SIZE);
    }
    return "";
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
        std::cerr << "Cannot set default address: address not found in wallet" << std::endl;
    }
}

std::string Wallet::getDefaultAddress() const {
    return defaultAddress;
}

bool Wallet::signTransaction(Transaction& transaction, const std::string& fromAddress) const {
    // Check if we have the private key for this address
    if (!hasAddress(fromAddress)) {
        std::cerr << "Address not found in wallet: " << fromAddress << std::endl;
        return false;
    }
    
    // Verify that the transaction sender matches the fromAddress
    if (transaction.getSender() != fromAddress) {
        std::cerr << "Transaction sender doesn't match fromAddress" << std::endl;
        return false;
    }
    
    // Get the private key for signing
    ECCrypto::PrivateKey privateKey = getPrivateKey(fromAddress);
    
    // Sign the transaction
    return transaction.signTransaction(privateKey);
}

bool Wallet::verifyTransaction(const Transaction& transaction, const std::string& address) const {
    // Check if we have the public key for this address
    if (!hasAddress(address)) {
        std::cerr << "Address not found in wallet: " << address << std::endl;
        return false;
    }
    
    // Get the public key for verification
    ECCrypto::PublicKey publicKey = getPublicKey(address);
    
    // Verify the transaction signature
    return transaction.verifySignature(publicKey);
}

bool Wallet::saveToFile(const std::string& filename, const std::string& password) const {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return false;
        }
        
        // Simple format: each line contains address:privateKeyHex:publicKeyHex
        // In a real implementation, you would encrypt this with the password
        
        // Write default address first
        file << "DEFAULT:" << defaultAddress << std::endl;
        
        // Write each key pair
        for (const auto& pair : keyPairs) {
            const std::string& address = pair.first;
            const auto& keyPair = pair.second;
            
            std::string privateKeyHex = ECCrypto::bytesToHex(keyPair->privateKey.data(), ECCrypto::PRIVATE_KEY_SIZE);
            std::string publicKeyHex = ECCrypto::bytesToHex(keyPair->publicKey.data(), ECCrypto::PUBLIC_KEY_SIZE);
            
            file << address << ":" << privateKeyHex << ":" << publicKeyHex << std::endl;
        }
        
        file.close();
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error saving wallet to file: " << e.what() << std::endl;
        return false;
    }
}

bool Wallet::loadFromFile(const std::string& filename, const std::string& password) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for reading: " << filename << std::endl;
            return false;
        }
        
        // Clear existing data
        clear();
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            // Parse default address line
            if (line.substr(0, 8) == "DEFAULT:") {
                defaultAddress = line.substr(8);
                continue;
            }
            
            // Parse key pair line: address:privateKeyHex:publicKeyHex
            size_t firstColon = line.find(':');
            size_t secondColon = line.find(':', firstColon + 1);
            
            if (firstColon == std::string::npos || secondColon == std::string::npos) {
                std::cerr << "Invalid line format in wallet file" << std::endl;
                continue;
            }
            
            std::string address = line.substr(0, firstColon);
            std::string privateKeyHex = line.substr(firstColon + 1, secondColon - firstColon - 1);
            std::string publicKeyHex = line.substr(secondColon + 1);
            
            // Import the key pair
            if (!importKeyPair(privateKeyHex, publicKeyHex)) {
                std::cerr << "Failed to import key pair for address: " << address << std::endl;
                continue;
            }
        }
        
        file.close();
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading wallet from file: " << e.what() << std::endl;
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
    ss << "  Total Addresses: " << keyPairs.size() << std::endl;
    ss << "  Default Address: " << (defaultAddress.empty() ? "None" : defaultAddress) << std::endl;
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

} // namespace wallet