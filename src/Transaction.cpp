#include "include/Transaction.h"
#include "include/utils.h"
#include "include/ECCrypto.h"
#include <format>
#include <print>

Transaction::Transaction(const std::string& from, const std::string& to, double value)
    : sender(from), receiver(to), amount(value), timestamp(utils::getCurrentTimestamp()) {
    signature = "";
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
    if (signature.empty() || address.empty()) {
        return false;
    }
    
    // This is a simplified approach - in a real implementation, you would need
    // to store public keys separately or use a more sophisticated verification method
    // For now, we'll check if the sender address matches the provided address
    return sender == address;
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
    
    // Check if transaction is signed
    if (signature.empty()) {
        std::println(stderr, "Invalid transaction: Transaction not signed");
        return false;
    }
    
    // In a real implementation, we would verify the signature here
    // For now, we'll just check that a signature exists
    // Full verification requires the public key of the sender
    return true;
}

std::string Transaction::getTransactionData() const {
    return std::format("{}:{}:{:.8f}:{}", sender, receiver, amount, timestamp);
}
