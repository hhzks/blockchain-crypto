#pragma once

#include "ECCrypto.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

// Forward declarations
class Transaction;

namespace wallet {
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
    std::string generateNewAddress();

    bool importKeyPair(const std::string& privateKeyHex, const std::string& publicKeyHex);
    bool importPrivateKey(const std::string& privateKeyHex);

    ECCrypto::PrivateKey getPrivateKey(const std::string& address) const;
    ECCrypto::PublicKey getPublicKey(const std::string& address) const;
    std::string getPrivateKeyHex(const std::string& address) const;
    std::string getPublicKeyHex(const std::string& address) const;

    bool hasAddress(const std::string& address) const;
    std::vector<std::string> getAllAddresses() const;
    void setDefaultAddress(const std::string& address);
    std::string getDefaultAddress() const;
    

    bool signTransaction(Transaction& transaction, const std::string& fromAddress) const;
    bool verifyTransaction(const Transaction& transaction, const std::string& address) const;

    bool saveToFile(const std::string& filename, const std::string& password) const;
    bool loadFromFile(const std::string& filename, const std::string& password);
    

    void clear();
    std::string toString() const;
};


namespace utils {

    std::unique_ptr<Wallet> createRandomWallet();
    bool isValidAddress(const std::string& address);
    std::string generateMnemonic();
    std::pair<ECCrypto::PrivateKey, ECCrypto::PublicKey> recoverFromMnemonic(const std::string& mnemonic);
}

} // namespace wallet