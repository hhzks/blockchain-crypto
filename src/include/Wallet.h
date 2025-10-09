#pragma once

#include "ECCrypto.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

// Forward declarations
class Transaction;

namespace wallet {

/**
 * Represents a wallet that can hold multiple key pairs and manage addresses
 */
class Wallet {
private:
    struct KeyPair {
        ECCrypto::PrivateKey privateKey;
        ECCrypto::PublicKey publicKey;
        std::string address;
    };
    
    std::unordered_map<std::string, std::unique_ptr<KeyPair>> keyPairs;
    std::string defaultAddress;

public:
    /**
     * Generate a new key pair and return the address
     */
    std::string generateNewAddress();
    
    /**
     * Import a key pair from hex strings
     */
    bool importKeyPair(const std::string& privateKeyHex, const std::string& publicKeyHex);
    
    /**
     * Import only a private key (public key will be derived)
     */
    bool importPrivateKey(const std::string& privateKeyHex);
    
    /**
     * Get the private key for an address
     */
    ECCrypto::PrivateKey getPrivateKey(const std::string& address) const;
    
    /**
     * Get the public key for an address
     */
    ECCrypto::PublicKey getPublicKey(const std::string& address) const;
    
    /**
     * Get private key as hex string
     */
    std::string getPrivateKeyHex(const std::string& address) const;
    
    /**
     * Get public key as hex string
     */
    std::string getPublicKeyHex(const std::string& address) const;
    
    /**
     * Check if wallet contains an address
     */
    bool hasAddress(const std::string& address) const;
    
    /**
     * Get all addresses in the wallet
     */
    std::vector<std::string> getAllAddresses() const;
    
    /**
     * Set default address for transactions
     */
    void setDefaultAddress(const std::string& address);
    
    /**
     * Get default address
     */
    std::string getDefaultAddress() const;
    
    /**
     * Sign a transaction with the key for a specific address
     */
    bool signTransaction(Transaction& transaction, const std::string& fromAddress) const;
    
    /**
     * Verify a transaction signature against an address
     */
    bool verifyTransaction(const Transaction& transaction, const std::string& address) const;
    
    /**
     * Save wallet to file (encrypted with password)
     */
    bool saveToFile(const std::string& filename, const std::string& password) const;
    
    /**
     * Load wallet from file (decrypted with password)
     */
    bool loadFromFile(const std::string& filename, const std::string& password);
    
    /**
     * Clear all keys from wallet
     */
    void clear();
    
    /**
     * Get wallet summary
     */
    std::string toString() const;
};

/**
 * Utility functions for key management
 */
namespace utils {
    /**
     * Generate a random wallet with one address
     */
    std::unique_ptr<Wallet> createRandomWallet();
    
    /**
     * Validate address format
     */
    bool isValidAddress(const std::string& address);
    
    /**
     * Generate a mnemonic phrase for key backup (simplified version)
     */
    std::string generateMnemonic();
    
    /**
     * Recover keys from mnemonic phrase (simplified version)
     */
    std::pair<ECCrypto::PrivateKey, ECCrypto::PublicKey> recoverFromMnemonic(const std::string& mnemonic);
}

} // namespace wallet