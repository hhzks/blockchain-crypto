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

        // Private keys should not outlive the wallet in freed heap memory.
        // Written through a volatile pointer so the compiler cannot drop the
        // stores as dead.
        ~KeyPair() {
            volatile uint8_t* p = private_key.data();
            for (size_t i = 0; i < private_key.size(); i++) {
                p[i] = 0;
            }
        }
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

    // The keystore is encrypted with `password` (see keystore.h for the
    // construction and its limits). Loading with the wrong password fails and
    // leaves the wallet's existing keys untouched.
    bool saveToFile(const std::string& filename, const std::string& password) const;
    bool loadFromFile(const std::string& filename, const std::string& password);

    void clear();
    std::string toString() const;
};

namespace utils {
    // A wallet holding one freshly generated key, set as the default address.
    std::unique_ptr<Wallet> createRandomWallet();

    // True for the output shape of ECCrypto::deriveAddress: exactly 40
    // lowercase hex characters, with no "0x" prefix.
    bool isValidAddress(const std::string& address);
}

}
