#include "include/Block.h"
#include "include/utils.h"
#include <sstream>
#include <iostream>
#include <chrono>

Block::Block(int idx, const std::string& prev_hash, int block_difficulty)
    : Block(idx, prev_hash, block_difficulty, utils::getCurrentTimestamp()) {
}

Block::Block(int idx, const std::string& prev_hash, int block_difficulty,
             long long block_timestamp)
    : index(idx), timestamp(block_timestamp), previous_hash(prev_hash),
      difficulty(block_difficulty), nonce(0) {
    hash = "";
    merkle_root = "";
    calculateMerkleRoot();
}

void Block::setMinedState(int mined_nonce, const std::string& mined_hash) {
    nonce = mined_nonce;
    hash = mined_hash;
}

void Block::addTransaction(std::shared_ptr<Transaction> transaction) {
    if (transaction && transaction->isValid()) {
        transactions.push_back(transaction);
        calculateMerkleRoot(); // Recalculate merkle root after adding transaction
    }
}

std::string Block::calculateHash() const {
    std::stringstream ss;
    ss << index << timestamp << previous_hash << merkle_root << difficulty << nonce;
    
    // Include all transaction hashes
    for (const auto& transaction : transactions) {
        ss << transaction->calculateHash();
    }
    
    return utils::sha256(ss.str());
}

void Block::mineBlock() {
    std::cout << "Mining block " << index << " with difficulty " << difficulty << "..." << std::endl;
    
    std::string target(difficulty, '0');
    nonce = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    do {
        nonce++;
        hash = calculateHash();
        
        // Print progress every 100000 attempts
        if (nonce % 100000 == 0) {
            std::cout << "Nonce: " << nonce << ", Hash: " << hash.substr(0, 20) << "..." << std::endl;
        }
        
    } while (hash.substr(0, difficulty) != target);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Block mined successfully!" << std::endl;
    std::cout << "Hash: " << hash << std::endl;
    std::cout << "Nonce: " << nonce << std::endl;
    std::cout << "Mining time: " << duration.count() << " ms" << std::endl;
    std::cout << std::endl;
}

void Block::calculateMerkleRoot() {
    if (transactions.empty()) {
        merkle_root = utils::sha256("");
        return;
    }
    
    std::vector<std::string> tx_hashes;
    for (const auto& tx : transactions) {
        tx_hashes.push_back(tx->calculateHash());
    }
    
    merkle_root = utils::calculateMerkleRoot(tx_hashes);
}

bool Block::isValid() const {
    // Check if hash is correctly calculated
    if (hash != calculateHash()) {
        std::cout << "Invalid block: Hash mismatch" << std::endl;
        return false;
    }
    
    // Check proof of work using the block's stored difficulty
    if (!utils::checkProofOfWork(hash, difficulty)) {
        std::cout << "Invalid block: Proof of work not satisfied for difficulty " << difficulty << std::endl;
        return false;
    }
    
    // Validate all transactions
    for (const auto& transaction : transactions) {
        if (!transaction->isValid()) {
            std::cout << "Invalid block: Contains invalid transaction" << std::endl;
            return false;
        }
    }
    
    std::vector<std::string> tx_hashes;
    for (const auto& tx : transactions) {
        tx_hashes.push_back(tx->calculateHash());
    }
    
    std::string computed_root = utils::calculateMerkleRoot(tx_hashes);
    if (merkle_root != computed_root) {
        std::cout << "Invalid block: Merkle root mismatch" << std::endl;
        return false;
    }
    
    return true;
}

bool Block::isValidWithDifficulty(int requiredDifficulty) const {
    // First check if the stored difficulty matches the required difficulty
    if (difficulty != requiredDifficulty) {
        std::cout << "Invalid block: Difficulty " << difficulty 
                  << " doesn't match required " << requiredDifficulty << std::endl;
        return false;
    }
    
    // Then perform standard validation
    return isValid();
}

bool Block::isMined(int difficulty) const {
    return utils::checkProofOfWork(hash, difficulty);
}

std::string Block::toString() const {
    std::stringstream ss;
    ss << "Block " << index << " {" << std::endl;
    ss << "  Timestamp: " << timestamp << std::endl;
    ss << "  Previous Hash: " << previous_hash << std::endl;
    ss << "  Merkle Root: " << merkle_root << std::endl;
    ss << "  Hash: " << hash << std::endl;
    ss << "  Nonce: " << nonce << std::endl;
    ss << "  Transactions (" << transactions.size() << "):" << std::endl;
    
    for (size_t i = 0; i < transactions.size(); i++) {
        ss << "    " << (i + 1) << ". From: " << transactions[i]->getSender() 
           << " To: " << transactions[i]->getReceiver() 
           << " Amount: " << transactions[i]->getAmount() << std::endl;
    }
    ss << "}" << std::endl;
    return ss.str();
}
