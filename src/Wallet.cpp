#include "include/Wallet.h"
#include "include/Transaction.h"
#include "include/utils.h"
#include "include/sha.h"
#include "include/ECCrypto.h"
#include <format>
#include <print>
#include <fstream>
#include <random>
#include <algorithm>

namespace wallet {

std::string Wallet::generateNewAddress() {
    try {
        auto ecc_key_pair = ECCrypto::generateKeyPair();
        if (!ecc_key_pair) {
            std::println(stderr, "Failed to generate ECC key pair");
            return "";
        }
        
        auto kp = std::make_unique<KeyPair>();
        
        if (ECCrypto::hexToBytes(ecc_key_pair->private_key_hex, kp->private_key.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE) {
            std::println(stderr, "Failed to convert private key to bytes");
            return "";
        }
        
        if (ECCrypto::hexToBytes(ecc_key_pair->public_key_hex, kp->public_key.data(), ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
            std::println(stderr, "Failed to convert public key to bytes");
            return "";
        }
        
        kp->address = ecc_key_pair->address;
        
        std::string address = kp->address;
        key_pairs[address] = std::move(kp);
        
        if (default_address.empty()) {
            default_address = address;
        }
        
        return address;
    }
    catch (const std::exception& e) {
        std::println(stderr, "Error generating new address: {}", e.what());
        return "";
    }
}

bool Wallet::importKeyPair(const std::string& priv_key_hex, const std::string& pub_key_hex) {
    try {
        if (priv_key_hex.length() != ECCrypto::PRIVATE_KEY_SIZE * 2 || 
            pub_key_hex.length() != ECCrypto::PUBLIC_KEY_SIZE * 2) {
            std::println(stderr, "Invalid key length");
            return false;
        }
        
        ECCrypto::PrivateKey priv_key;
        ECCrypto::PublicKey pub_key;
        
        if (ECCrypto::hexToBytes(priv_key_hex, priv_key.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE ||
            ECCrypto::hexToBytes(pub_key_hex, pub_key.data(), ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
            std::println(stderr, "Failed to convert hex keys to bytes");
            return false;
        }
        
        if (!ECCrypto::isValidPrivateKey(priv_key) || !ECCrypto::isValidPublicKey(pub_key)) {
            std::println(stderr, "Invalid key pair");
            return false;
        }
        
        std::string address = ECCrypto::deriveAddress(pub_key);
        
        auto kp = std::make_unique<KeyPair>();
        kp->private_key = priv_key;
        kp->public_key = pub_key;
        kp->address = address;
        
        key_pairs[address] = std::move(kp);
        
        if (default_address.empty()) {
            default_address = address;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::println(stderr, "Error importing key pair: {}", e.what());
        return false;
    }
}

bool Wallet::importPrivateKey(const std::string& priv_key_hex) {
    try {
        auto ecc_key_pair = ECCrypto::keyPairFromPrivateKeyHex(priv_key_hex);
        if (!ecc_key_pair) {
            std::println(stderr, "Failed to generate key pair from private key");
            return false;
        }
        
        auto kp = std::make_unique<KeyPair>();
        
        if (ECCrypto::hexToBytes(ecc_key_pair->private_key_hex, kp->private_key.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE) {
            std::println(stderr, "Failed to convert private key to bytes");
            return false;
        }
        
        if (ECCrypto::hexToBytes(ecc_key_pair->public_key_hex, kp->public_key.data(), ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
            std::println(stderr, "Failed to convert public key to bytes");
            return false;
        }
        
        kp->address = ecc_key_pair->address;
        
        std::string address = kp->address;
        key_pairs[address] = std::move(kp);
        
        if (default_address.empty()) {
            default_address = address;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::println(stderr, "Error importing private key: {}", e.what());
        return false;
    }
}

ECCrypto::PrivateKey Wallet::getPrivateKey(const std::string& address) const {
    auto it = key_pairs.find(address);
    if (it != key_pairs.end()) {
        return it->second->private_key;
    }
    return ECCrypto::PrivateKey{};
}

ECCrypto::PublicKey Wallet::getPublicKey(const std::string& address) const {
    auto it = key_pairs.find(address);
    if (it != key_pairs.end()) {
        return it->second->public_key;
    }
    return ECCrypto::PublicKey{};
}

std::string Wallet::getPrivateKeyHex(const std::string& address) const {
    auto it = key_pairs.find(address);
    if (it != key_pairs.end()) {
        return ECCrypto::bytesToHex(it->second->private_key.data(), ECCrypto::PRIVATE_KEY_SIZE);
    }
    return "";
}

std::string Wallet::getPublicKeyHex(const std::string& address) const {
    auto it = key_pairs.find(address);
    if (it != key_pairs.end()) {
        return ECCrypto::bytesToHex(it->second->public_key.data(), ECCrypto::PUBLIC_KEY_SIZE);
    }
    return "";
}

bool Wallet::hasAddress(const std::string& address) const {
    return key_pairs.contains(address);
}

std::vector<std::string> Wallet::getAllAddresses() const {
    std::vector<std::string> addresses;
    addresses.reserve(key_pairs.size());
    
    for (const auto& pair : key_pairs) {
        addresses.push_back(pair.first);
    }
    
    return addresses;
}

void Wallet::setDefaultAddress(const std::string& address) {
    if (hasAddress(address)) {
        default_address = address;
    } else {
        std::println(stderr, "Cannot set default address: address not found in wallet");
    }
}

std::string Wallet::getDefaultAddress() const {
    return default_address;
}

bool Wallet::signTransaction(Transaction& transaction, const std::string& from_address) const {
    if (!hasAddress(from_address)) {
        std::println(stderr, "Address not found in wallet: {}", from_address);
        return false;
    }
    
    if (transaction.getSender() != from_address) {
        std::println(stderr, "Transaction sender doesn't match from_address");
        return false;
    }
    
    ECCrypto::PrivateKey priv_key = getPrivateKey(from_address);
    return transaction.signTransaction(priv_key);
}

bool Wallet::verifyTransaction(const Transaction& transaction, const std::string& address) const {
    if (!hasAddress(address)) {
        std::println(stderr, "Address not found in wallet: {}", address);
        return false;
    }
    
    ECCrypto::PublicKey pub_key = getPublicKey(address);
    return transaction.verifySignature(pub_key);
}

bool Wallet::saveToFile(const std::string& filename, const std::string& password) const {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::println(stderr, "Failed to open file for writing: {}", filename);
            return false;
        }
        
        file << "DEFAULT:" << default_address << std::endl;
        
        for (const auto& pair : key_pairs) {
            const std::string& address = pair.first;
            const auto& kp = pair.second;
            
            std::string priv_hex = ECCrypto::bytesToHex(kp->private_key.data(), ECCrypto::PRIVATE_KEY_SIZE);
            std::string pub_hex = ECCrypto::bytesToHex(kp->public_key.data(), ECCrypto::PUBLIC_KEY_SIZE);
            
            file << address << ":" << priv_hex << ":" << pub_hex << std::endl;
        }
        
        file.close();
        return true;
    }
    catch (const std::exception& e) {
        std::println(stderr, "Error saving wallet to file: {}", e.what());
        return false;
    }
}

bool Wallet::loadFromFile(const std::string& filename, const std::string& password) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::println(stderr, "Failed to open file for reading: {}", filename);
            return false;
        }
        
        clear();
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            if (line.substr(0, 8) == "DEFAULT:") {
                default_address = line.substr(8);
                continue;
            }
            
            size_t first_colon = line.find(':');
            size_t second_colon = line.find(':', first_colon + 1);
            
            if (first_colon == std::string::npos || second_colon == std::string::npos) {
                std::println(stderr, "Invalid line format in wallet file");
                continue;
            }
            
            std::string address = line.substr(0, first_colon);
            std::string priv_hex = line.substr(first_colon + 1, second_colon - first_colon - 1);
            std::string pub_hex = line.substr(second_colon + 1);
            
            if (!importKeyPair(priv_hex, pub_hex)) {
                std::println(stderr, "Failed to import key pair for address: {}", address);
                continue;
            }
        }
        
        file.close();
        return true;
    }
    catch (const std::exception& e) {
        std::println(stderr, "Error loading wallet from file: {}", e.what());
        return false;
    }
}

void Wallet::clear() {
    key_pairs.clear();
    default_address.clear();
}

std::string Wallet::toString() const {
    std::string result = std::format(
        "Wallet Summary:\n  Total Addresses: {}\n  Default Address: {}\n  Addresses:\n",
        key_pairs.size(),
        default_address.empty() ? "None" : default_address
    );

    for (const auto& [address, kp] : key_pairs) {
        result += std::format("    {}{}\n", address, address == default_address ? " (default)" : "");
    }

    return result;
}

} // namespace wallet