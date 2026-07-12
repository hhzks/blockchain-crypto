#include "include/Transaction.h"
#include "include/utils.h"
#include "include/ECCrypto.h"
#include <format>
#include <print>

Transaction::Transaction(const std::string& from, const std::string& to, double value)
    : sender(from), receiver(to), amount(value), timestamp(utils::getCurrentTimestamp()) {
    signature = "";
}

Transaction::Transaction(const std::string& from, const std::string& to, double value,
                         long long tx_timestamp, const std::string& tx_signature)
    : sender(from), receiver(to), amount(value), timestamp(tx_timestamp),
      signature(tx_signature) {
}

std::string Transaction::calculateHash() const {
    return utils::sha256(std::format("{}:{}:{:.8f}:{}", sender, receiver, amount, timestamp));
}

bool Transaction::signTransaction(const ECCrypto::PrivateKey& private_key) {
    if (!ECCrypto::isValidPrivateKey(private_key)) {
        std::println(stderr, "Invalid private key for transaction signing");
        return false;
    }
    
    try {
        std::string tx_data = getTransactionData();
        ECCrypto::Signature sig = ECCrypto::signMessage(tx_data, private_key);
        signature = ECCrypto::signatureToHex(sig);
        // Attach the sender's public key so the transaction can be verified
        // downstream from the address alone. deriveAddress(pubkey) == sender is
        // enforced in isValid(). The key is NOT part of the signed data.
        BigInt priv = ECCrypto::bytes32ToBigInt(private_key.data());
        auto kp = ECCrypto::keyPairFromPrivateKey(priv);
        if (kp) {
            sender_pubkey = kp->public_key_hex;
        }
        std::println(stderr, "Transaction signed successfully");
        return true;
        
    } catch (const std::exception& e) {
        std::println(stderr, "Error signing transaction: {}", e.what());
        return false;
    }
}

bool Transaction::signTransaction(const std::string& private_key_hex) {
    if (private_key_hex.length() != ECCrypto::PRIVATE_KEY_SIZE * 2) {
        std::println(stderr, "Invalid private key hex length");
        return false;
    }
    
    ECCrypto::PrivateKey priv_key;
    if (ECCrypto::hexToBytes(private_key_hex, priv_key.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE) {
        std::println(stderr, "Failed to parse private key hex");
        return false;
    }
    
    return signTransaction(priv_key);
}

bool Transaction::verifySignature(const ECCrypto::PublicKey& public_key) const {
    if (signature.empty()) {
        return false;
    }
    
    if (!ECCrypto::isValidPublicKey(public_key)) {
        return false;
    }
    
    try {
        ECCrypto::Signature sig = ECCrypto::signatureFromHex(signature);
        std::string tx_data = getTransactionData();
        return ECCrypto::verifyMessageSignature(tx_data, sig, public_key);
        
    } catch (const std::exception& e) {
        std::println(stderr, "Error verifying signature: {}", e.what());
        return false;
    }
}

bool Transaction::verifySignature(const std::string& public_key_hex) const {
    if (public_key_hex.length() != ECCrypto::PUBLIC_KEY_SIZE * 2) {
        return false;
    }
    
    ECCrypto::PublicKey pub_key;
    if (ECCrypto::hexToBytes(public_key_hex, pub_key.data(), ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
        return false;
    }
    
    return verifySignature(pub_key);
}

bool Transaction::verifySignatureByAddress(const std::string& address) const {
    if (signature.empty() || address.empty() || sender_pubkey.empty()) {
        return false;
    }
    // The attached key must belong to `address`, and the signature must verify.
    std::string derived = derivedAddressFromKey();
    if (derived.empty() || derived != address) {
        return false;
    }
    return verifySignature(sender_pubkey);
}

std::string Transaction::toString() const {
    return std::format(
        "Transaction {{\n  From: {}\n  To: {}\n  Amount: {}\n  Timestamp: {}\n  Hash: {}\n  Signature: {}\n}}\n",
        sender, receiver, amount, timestamp, calculateHash(),
        signature.empty() ? "Not signed" : signature.substr(0, 16) + "..."
    );
}

bool Transaction::isValid() const {
    // Check basic validity conditions
    if (sender.empty() || receiver.empty()) {
        std::println(stderr, "Invalid transaction: Empty sender or receiver address");
        return false;
    }
    
    if (amount <= 0) {
        std::println(stderr, "Invalid transaction: Amount must be positive");
        return false;
    }
    
    if (sender == receiver) {
        std::println(stderr, "Invalid transaction: Cannot send to yourself");
        return false;
    }
    
    // For mining reward transactions (from "system"), signature verification is skipped
    if (sender == "system") {
        return true;
    }

    // Must be signed with a well-formed signature.
    if (signature.empty()) {
        std::println(stderr, "Invalid transaction: Transaction not signed");
        return false;
    }
    if (signature.length() != ECCrypto::SIGNATURE_SIZE * 2) {
        std::println(stderr, "Invalid transaction: Malformed signature");
        return false;
    }

    // The attached public key must hash to the sender address (binds key->address).
    std::string derived = derivedAddressFromKey();
    if (derived.empty() || derived != sender) {
        std::println(stderr, "Invalid transaction: Public key does not match sender address");
        return false;
    }

    // Finally, the signature must verify against that public key.
    if (!verifySignature(sender_pubkey)) {
        std::println(stderr, "Invalid transaction: Signature verification failed");
        return false;
    }

    return true;
}

std::string Transaction::getTransactionData() const {
    return std::format("{}:{}:{:.8f}:{}", sender, receiver, amount, timestamp);
}

std::string Transaction::derivedAddressFromKey() const {
    if (sender_pubkey.length() != ECCrypto::PUBLIC_KEY_SIZE * 2) {
        return "";
    }
    ECCrypto::PublicKey pub_key;
    if (ECCrypto::hexToBytes(sender_pubkey, pub_key.data(),
                             ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
        return "";
    }
    return ECCrypto::deriveAddress(pub_key);
}
