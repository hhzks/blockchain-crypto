#include "../include/Transaction.h"
#include "../include/utils.h"
#include <sstream>
#include <iostream>

Transaction::Transaction(const std::string& from, const std::string& to, double value)
    : sender(from), receiver(to), amount(value), timestamp(utils::getCurrentTimestamp()) {
    // Initialize signature as empty - will be set when transaction is signed
    signature = "";
}

std::string Transaction::calculateHash() const {
    std::stringstream ss;
    ss << sender << receiver << amount << timestamp;
    return utils::sha256(ss.str());
}

void Transaction::signTransaction(const std::string& privateKey) {
    // Simplified signing - in real implementation, this would use proper cryptographic signing
    std::string transactionData = sender + receiver + std::to_string(amount) + std::to_string(timestamp);
    signature = utils::sha256(transactionData + privateKey);
}

bool Transaction::verifySignature(const std::string& publicKey) const {
    if (signature.empty()) {
        return false;
    }
    
    // Simplified verification - in real implementation, this would use proper cryptographic verification
    std::string transactionData = sender + receiver + std::to_string(amount) + std::to_string(timestamp);
    std::string expectedSignature = utils::sha256(transactionData + publicKey);
    
    return signature == expectedSignature;
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
    
    return true;
}
