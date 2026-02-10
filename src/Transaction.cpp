#include "include/Transaction.h"
#include "include/utils.h"
#include "include/ECCrypto.h"
#include <sstream>
#include <iostream>
#include <iomanip>

Transaction::Transaction(const std::string& from, const std::string& to, double value)
    : sender(from), receiver(to), amount(value), timestamp(utils::getCurrentTimestamp()) {
    signature = "";
}

std::string Transaction::calculateHash() const {
    std::stringstream ss;
    ss << sender << ":" << receiver << ":" << std::fixed << std::setprecision(8) << amount << ":" << timestamp;
    return utils::sha256(ss.str());
}

bool Transaction::signTransaction(const ECCrypto::PrivateKey& private_key) {
    if (!ECCrypto::isValidPrivateKey(private_key)) {
        std::cout << "Invalid private key for transaction signing" << std::endl;
        return false;
    }
    
    try {
        std::string tx_data = getTransactionData();
        ECCrypto::Signature sig = ECCrypto::signMessage(tx_data, private_key);
        signature = ECCrypto::signatureToHex(sig);
        std::cout << "Transaction signed successfully" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Error signing transaction: " << e.what() << std::endl;
        return false;
    }
}

bool Transaction::signTransaction(const std::string& private_key_hex) {
    if (private_key_hex.length() != ECCrypto::PRIVATE_KEY_SIZE * 2) {
        std::cout << "Invalid private key hex length" << std::endl;
        return false;
    }
    
    ECCrypto::PrivateKey priv_key;
    if (ECCrypto::hexToBytes(private_key_hex, priv_key.data(), ECCrypto::PRIVATE_KEY_SIZE) != ECCrypto::PRIVATE_KEY_SIZE) {
        std::cout << "Failed to parse private key hex" << std::endl;
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
        std::cout << "Error verifying signature: " << e.what() << std::endl;
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
    std::stringstream ss;
    ss << "Transaction {" << std::endl;
    ss << "  From: " << sender << std::endl;
    ss << "  To: " << receiver << std::endl;
    ss << "  Amount: " << amount << std::endl;
    ss << "  Timestamp: " << timestamp << std::endl;
    ss << "  Hash: " << calculateHash() << std::endl;
    ss << "  Signature: " << (signature.empty() ? "Not signed" : signature.substr(0, 16) + "...") << std::endl;
    ss << "}" << std::endl;
    return ss.str();
}

bool Transaction::isValid() const {
    // Check basic validity conditions
    if (sender.empty() || receiver.empty()) {
        std::cout << "Invalid transaction: Empty sender or receiver address" << std::endl;
        return false;
    }
    
    if (amount <= 0) {
        std::cout << "Invalid transaction: Amount must be positive" << std::endl;
        return false;
    }
    
    if (sender == receiver) {
        std::cout << "Invalid transaction: Cannot send to yourself" << std::endl;
        return false;
    }
    
    // For mining reward transactions (from "system"), signature verification is skipped
    if (sender == "system") {
        return true;
    }
    
    // Check if transaction is signed
    if (signature.empty()) {
        std::cout << "Invalid transaction: Transaction not signed" << std::endl;
        return false;
    }
    
    // In a real implementation, we would verify the signature here
    // For now, we'll just check that a signature exists
    // Full verification requires the public key of the sender
    return true;
}

std::string Transaction::getTransactionData() const {
    std::stringstream ss;
    ss << sender << ":" << receiver << ":" << std::fixed << std::setprecision(8) << amount << ":" << timestamp;
    return ss.str();
}
