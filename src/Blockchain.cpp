#include "include/Blockchain.h"
#include "include/utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>

Blockchain::Blockchain() : difficulty(4), mining_reward(100.0) {
    chain.push_back(createGenesisBlock());
}

Blockchain::Blockchain(int initial_difficulty, double initial_reward)
    : difficulty(initial_difficulty), mining_reward(initial_reward) {
    chain.push_back(createGenesisBlock());
}

std::shared_ptr<Block> Blockchain::createGenesisBlock() {
    auto genesis = std::make_shared<Block>(0, "0", INITIAL_DIFFICULTY);
    auto genesis_tx = std::make_shared<Transaction>("system", "genesis", 0);
    genesis->addTransaction(genesis_tx);
    genesis->mineBlock();
    std::cout << "Genesis block created!" << std::endl;
    return genesis;
}

std::shared_ptr<Block> Blockchain::getLatestBlock() const {
    return chain.back();
}

void Blockchain::addTransaction(std::shared_ptr<Transaction> transaction) {
    if (!transaction->isValid()) {
        std::cout << "Invalid transaction rejected!" << std::endl;
        return;
    }
    
    if (transactionExists(transaction)) {
        std::cout << "Transaction already exists in blockchain!" << std::endl;
        return;
    }
    
    if (transaction->getSender() != "system") {
        double balance = getBalance(transaction->getSender());
        if (balance < transaction->getAmount()) {
            std::cout << "Transaction rejected: Insufficient balance. Balance: " 
                      << balance << ", Required: " << transaction->getAmount() << std::endl;
            return;
        }
    }
    
    pending_transactions.push_back(transaction);
    std::cout << "Transaction added to pending pool" << std::endl;
}

void Blockchain::minePendingTransactions(const std::string& reward_address) {
    if (pending_transactions.empty()) {
        std::cout << "No pending transactions to mine!" << std::endl;
        return;
    }
    
    int required_difficulty = calculateRequiredDifficulty();
    
    std::cout << "Starting to mine block with " << pending_transactions.size() 
              << " transactions at difficulty " << required_difficulty << "..." << std::endl;
    
    auto new_block = std::make_shared<Block>(
        static_cast<int>(chain.size()),
        getLatestBlock()->getHash(),
        required_difficulty
    );
    
    for (auto& tx : pending_transactions) {
        new_block->addTransaction(tx);
    }
    
    auto reward_tx = std::make_shared<Transaction>("system", reward_address, mining_reward);
    new_block->addTransaction(reward_tx);
    
    new_block->mineBlock();
    
    if (!validateBlockDifficulty(new_block)) {
        std::cout << "CRITICAL ERROR: Mined block has incorrect difficulty!" << std::endl;
        return;
    }
    
    chain.push_back(new_block);
    pending_transactions.clear();
    updateBalances();
    
    std::cout << "Block mined and added to blockchain!" << std::endl;
}

int Blockchain::calculateRequiredDifficulty() const {
    if (chain.size() < DIFFICULTY_ADJUSTMENT_INTERVAL) {
        return INITIAL_DIFFICULTY;
    }
    
    size_t last_idx = chain.size() - DIFFICULTY_ADJUSTMENT_INTERVAL;
    auto last_block = chain[last_idx];
    auto latest = chain.back();
    
    long long expected = TARGET_BLOCK_TIME * DIFFICULTY_ADJUSTMENT_INTERVAL;
    long long actual = latest->getTimestamp() - last_block->getTimestamp();
    int current = latest->getDifficulty();
    
    if (actual < expected / 2) {
        return current + 1;
    } else if (actual > expected * 2) {
        return std::max(1, current - 1);
    }
    
    return current;
}

bool Blockchain::validateBlockDifficulty(const std::shared_ptr<Block>& block) const {
    int required = calculateRequiredDifficultyAtHeight(block->getIndex());
    int actual = block->getDifficulty();
    
    if (actual != required) {
        std::cout << "Block " << block->getIndex() 
                  << " has incorrect difficulty: " << actual 
                  << " (required: " << required << ")" << std::endl;
        return false;
    }
    
    return true;
}

double Blockchain::getBalance(const std::string& address) {
    double balance = 0.0;
    
    // Calculate balance by going through all transactions in all blocks
    for (const auto& block : chain) {
        for (const auto& transaction : block->getTransactions()) {
            if (transaction->getReceiver() == address) {
                balance += transaction->getAmount();
            }
            if (transaction->getSender() == address) {
                balance -= transaction->getAmount();
            }
        }
    }
    
    return balance;
}

bool Blockchain::isChainValid() const {
    if (chain.size() < 1) {
        return false;
    }
    
    // Validate genesis block
    if (!chain[0]->isValidWithDifficulty(INITIAL_DIFFICULTY)) {
        std::cout << "Invalid genesis block" << std::endl;
        return false;
    }
    
    // Check each block
    for (size_t i = 1; i < chain.size(); i++) {
        const auto& currentBlock = chain[i];
        const auto& previousBlock = chain[i - 1];
        
        // Calculate what the difficulty should be at this height
        int requiredDifficulty = calculateRequiredDifficultyAtHeight(i);
        
        // Validate current block with required difficulty
        if (!currentBlock->isValidWithDifficulty(requiredDifficulty)) {
            std::cout << "Invalid block found at index " << i << std::endl;
            return false;
        }
        
        // Check if current block's previous hash matches previous block's hash
        if (currentBlock->getPreviousHash() != previousBlock->getHash()) {
            std::cout << "Chain broken at block " << i << std::endl;
            return false;
        }
        
        // Verify timestamp is reasonable (prevent time manipulation)
        if (currentBlock->getTimestamp() <= previousBlock->getTimestamp()) {
            std::cout << "Block " << i << " has invalid timestamp" << std::endl;
            return false;
        }
    }
    
    return true;
}

void Blockchain::printChain() const {
    std::cout << "=== BLOCKCHAIN ===" << std::endl;
    std::cout << "Chain length: " << chain.size() << " blocks" << std::endl;
    std::cout << "Difficulty: " << difficulty << std::endl;
    std::cout << "Mining reward: " << mining_reward << std::endl;
    std::cout << "Pending transactions: " << pending_transactions.size() << std::endl;
    std::cout << std::endl;
    
    for (const auto& block : chain) {
        std::cout << block->toString() << std::endl;
    }
    
    if (!pending_transactions.empty()) {
        std::cout << "=== PENDING TRANSACTIONS ===" << std::endl;
        for (size_t i = 0; i < pending_transactions.size(); i++) {
            std::cout << (i + 1) << ". " << pending_transactions[i]->toString() << std::endl;
        }
    }
}

bool Blockchain::saveToFile(const std::string& filename) const {
    std::ofstream file("blockchain_saves/" + filename);
    if (!file.is_open()) {
        std::cout << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    file << difficulty << std::endl;
    file << mining_reward << std::endl;
    file << chain.size() << std::endl;
    
    // Save each block
    for (const auto& block : chain) {
        file << block->getIndex() << std::endl;
        file << block->getTimestamp() << std::endl;
        file << block->getPreviousHash() << std::endl;
        file << block->getHash() << std::endl;
        file << block->getMerkleRoot() << std::endl;
        file << block->getDifficulty() << std::endl;
        file << block->getNonce() << std::endl;
        
        const auto& transactions = block->getTransactions();
        file << transactions.size() << std::endl;
        
        for (const auto& tx : transactions) {
            file << tx->getSender() << std::endl;
            file << tx->getReceiver() << std::endl;
            file << tx->getAmount() << std::endl;
            file << tx->getTimestamp() << std::endl;
            file << tx->getSignature() << std::endl;
        }
    }
    
    file.close();
    std::cout << "Blockchain saved to " << filename << std::endl;
    return true;
}

bool Blockchain::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }
    
    chain.clear();
    pending_transactions.clear();
    
    file >> difficulty >> mining_reward;
    
    size_t chain_size;
    file >> chain_size;
    
    for (size_t i = 0; i < chain_size; i++) {
        int index;
        long long timestamp;
        std::string prev_hash, hash, merkle_root;
        int block_difficulty, nonce;
        
        file >> index >> timestamp >> prev_hash >> hash >> merkle_root >> block_difficulty >> nonce;
        
        auto block = std::make_shared<Block>(index, prev_hash, block_difficulty);
        
        size_t tx_count;
        file >> tx_count;
        
        for (size_t j = 0; j < tx_count; j++) {
            std::string sender, receiver, signature;
            double amount;
            long long tx_timestamp;
            
            file >> sender >> receiver >> amount >> tx_timestamp >> signature;
            
            auto tx = std::make_shared<Transaction>(sender, receiver, amount);
            block->addTransaction(tx);
        }
        
        chain.push_back(block);
    }
    
    file.close();
    std::cout << "Blockchain loaded from " << filename << std::endl;
    return true;
}

void Blockchain::updateBalances() {
    balances.clear();
    
    for (const auto& block : chain) {
        for (const auto& transaction : block->getTransactions()) {
            // Credit receiver
            balances[transaction->getReceiver()] += transaction->getAmount();
            
            // Debit sender (except for system transactions)
            if (transaction->getSender() != "system") {
                balances[transaction->getSender()] -= transaction->getAmount();
            }
        }
    }
}

bool Blockchain::transactionExists(const std::shared_ptr<Transaction>& tx) const {
    std::string tx_hash = tx->calculateHash();
    
    for (const auto& block : chain) {
        for (const auto& existing : block->getTransactions()) {
            if (existing->calculateHash() == tx_hash) {
                return true;
            }
        }
    }
    
    for (const auto& pending : pending_transactions) {
        if (pending->calculateHash() == tx_hash) {
            return true;
        }
    }
    
    return false;
}

int Blockchain::calculateRequiredDifficultyAtHeight(int height) const {
    if (height == 0) {
        return INITIAL_DIFFICULTY; // Genesis block
    }
    
    if (height < DIFFICULTY_ADJUSTMENT_INTERVAL) {
        return INITIAL_DIFFICULTY;
    }
    
    // For blocks after the adjustment interval, we need to calculate based on timing
    // In a real implementation, we'd recalculate difficulty as it would have been at that height
    // For now, we'll simulate a progressive difficulty increase
    
    int adjustmentPeriods = height / DIFFICULTY_ADJUSTMENT_INTERVAL;
    int baseDifficulty = INITIAL_DIFFICULTY;
    
    // Simple simulation: difficulty increases every adjustment period
    // In reality, this would be calculated based on actual block timing at that height
    return std::min(baseDifficulty + adjustmentPeriods, 6); // Cap at difficulty 6
}
