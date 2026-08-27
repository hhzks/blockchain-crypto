#pragma once

#include <string>
#include <ctime>
#include "ECCrypto.h"
#include "money.h"

class Transaction {
private:
    std::string sender;
    std::string receiver;
    money::Amount amount;
    long long timestamp;
    std::string signature;
    std::string sender_pubkey;

public:
    Transaction(const std::string& from, const std::string& to, money::Amount value);
    // Restore constructor for deserialization: preserves the original
    // timestamp and signature so hashes and signature checks still match.
    Transaction(const std::string& from, const std::string& to, money::Amount value,
                long long tx_timestamp, const std::string& tx_signature);

    // Deleted, not overloaded: an implicit double->Amount conversion would
    // read 50.0 as 50 units (0.0000005 coin) instead of 50 coins, silently.
    Transaction(const std::string& from, const std::string& to, double value) = delete;
    Transaction(const std::string& from, const std::string& to, double value,
                long long tx_timestamp, const std::string& tx_signature) = delete;

    std::string getSender() const { return sender; }
    std::string getReceiver() const { return receiver; }
    money::Amount getAmount() const { return amount; }
    long long getTimestamp() const { return timestamp; }
    std::string getSignature() const { return signature; }
    std::string getSenderPublicKey() const { return sender_pubkey; }

    std::string calculateHash() const;
    bool signTransaction(const ECCrypto::PrivateKey& private_key);
    bool signTransaction(const std::string& private_key_hex);
    bool verifySignature(const ECCrypto::PublicKey& public_key) const;
    bool verifySignature(const std::string& public_key_hex) const;
    bool verifySignatureByAddress(const std::string& address) const;
    std::string toString() const;
    bool isValid() const;
    void setSignature(const std::string& sig) { signature = sig; }
    void setSenderPublicKey(const std::string& pubkey_hex) { sender_pubkey = pubkey_hex; }

private:
    std::string getTransactionData() const;
    std::string derivedAddressFromKey() const;
};
