#pragma once

#include "ECCrypto.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

class Transaction;

namespace wallet {

class Wallet {
private:
    struct KeyPair {
        ECCrypto::PrivateKey private_key;
        ECCrypto::PublicKey public_key;
        std::string address;
    };

    std::unordered_map<std::string, std::unique_ptr<KeyPair>> key_pairs;
    std::string default_address;

public:
    std::string generateNewAddress();
    bool importKeyPair(const std::string& private_key_hex, const std::string& public_key_hex);
    bool importPrivateKey(const std::string& private_key_hex);

    ECCrypto::PrivateKey getPrivateKey(const std::string& address) const;
    ECCrypto::PublicKey getPublicKey(const std::string& address) const;
    std::string getPrivateKeyHex(const std::string& address) const;
    std::string getPublicKeyHex(const std::string& address) const;

    bool hasAddress(const std::string& address) const;
    std::vector<std::string> getAllAddresses() const;
    void setDefaultAddress(const std::string& address);
    std::string getDefaultAddress() const;

    bool signTransaction(Transaction& transaction, const std::string& from_address) const;
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

}
