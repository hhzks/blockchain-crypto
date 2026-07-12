#include "include/Blockchain.h"
#include "include/utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <unordered_set>
#include <format>

Blockchain::Blockchain() : difficulty(INITIAL_DIFFICULTY), mining_reward(100.0) {
    chain.push_back(createGenesisBlock());
}

Blockchain::Blockchain(int initial_difficulty, double initial_reward)
    : difficulty(initial_difficulty), mining_reward(initial_reward) {
    chain.push_back(createGenesisBlock());
}

std::shared_ptr<Block> Blockchain::createGenesisBlock() {
    // The genesis block carries no transactions: a zero-amount marker tx
    // would be rejected by Block::addTransaction's validity check anyway.
    auto genesis = std::make_shared<Block>(0, "0", difficulty);
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
    return calculateRequiredDifficultyAtHeight(static_cast<int>(chain.size()));
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
    if (!chain[0]->isValidWithDifficulty(difficulty)) {
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
        
        // Verify timestamp is reasonable (prevent time manipulation).
        // Equal timestamps are allowed: timestamps have second granularity
        // and consecutive blocks can be mined within the same second.
        if (currentBlock->getTimestamp() < previousBlock->getTimestamp()) {
            std::cout << "Block " << i << " has invalid timestamp" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool Blockchain::addBlock(std::shared_ptr<Block> block) {
    if (!block) {
        return false;
    }

    const auto& tip = getLatestBlock();

    // Must extend the current tip at exactly the next height.
    if (block->getIndex() != static_cast<int>(chain.size())) {
        std::cout << "Rejected block: wrong index " << block->getIndex()
                  << " (expected " << chain.size() << ")" << std::endl;
        return false;
    }

    if (block->getPreviousHash() != tip->getHash()) {
        std::cout << "Rejected block: previous hash does not match tip" << std::endl;
        return false;
    }

    // Second-granularity timestamps; equal is allowed (matches isChainValid).
    if (block->getTimestamp() < tip->getTimestamp()) {
        std::cout << "Rejected block: timestamp precedes tip" << std::endl;
        return false;
    }

    int required = calculateRequiredDifficultyAtHeight(block->getIndex());
    if (!block->isValidWithDifficulty(required)) {
        std::cout << "Rejected block: failed validation" << std::endl;
        return false;
    }

    // Reward invariant: a legitimate block carries exactly one system
    // (mining-reward) transaction of the expected amount, matching how
    // minePendingTransactions builds blocks. Without this, a peer could mint
    // unlimited coins in a single crafted block, since system transactions are
    // exempt from signature verification and per-transaction validation alone
    // never bounds their count or amount.
    int system_tx_count = 0;
    for (const auto& tx : block->getTransactions()) {
        if (tx->getSender() == "system") {
            system_tx_count++;
            if (tx->getAmount() != mining_reward) {
                std::cout << "Rejected block: system reward amount "
                          << tx->getAmount() << " does not match expected "
                          << mining_reward << std::endl;
                return false;
            }
        }
    }
    if (system_tx_count != 1) {
        std::cout << "Rejected block: expected exactly one system reward "
                     "transaction, found " << system_tx_count << std::endl;
        return false;
    }

    chain.push_back(block);

    // Drop any pending transactions now included in the accepted block.
    if (!pending_transactions.empty()) {
        std::unordered_set<std::string> included;
        for (const auto& tx : block->getTransactions()) {
            included.insert(tx->calculateHash());
        }
        std::erase_if(pending_transactions,
                      [&included](const std::shared_ptr<Transaction>& tx) {
                          return included.contains(tx->calculateHash());
                      });
    }

    updateBalances();
    std::cout << "Block " << block->getIndex()
              << " accepted and added to chain" << std::endl;
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
    std::ofstream file(filename);
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
            // Fixed precision matching Transaction::calculateHash so the
            // reloaded amount reproduces the same hash.
            file << std::format("{:.8f}", tx->getAmount()) << std::endl;
            file << tx->getTimestamp() << std::endl;
            // "-" sentinel: an empty signature line would be skipped by
            // operator>> on load and corrupt the parse.
            file << (tx->getSignature().empty() ? "-" : tx->getSignature()) << std::endl;
            file << (tx->getSenderPublicKey().empty() ? "-" : tx->getSenderPublicKey()) << std::endl;
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

        auto block = std::make_shared<Block>(index, prev_hash, block_difficulty, timestamp);

        size_t tx_count;
        file >> tx_count;

        for (size_t j = 0; j < tx_count; j++) {
            std::string sender, receiver, signature, pubkey;
            double amount;
            long long tx_timestamp;

            file >> sender >> receiver >> amount >> tx_timestamp >> signature >> pubkey;
            if (signature == "-") {
                signature.clear();
            }
            if (pubkey == "-") {
                pubkey.clear();
            }

            auto tx = std::make_shared<Transaction>(sender, receiver, amount,
                                                    tx_timestamp, signature);
            tx->setSenderPublicKey(pubkey);
            block->addTransaction(tx);
        }

        block->setMinedState(nonce, hash);

        if (block->getMerkleRoot() != merkle_root) {
            std::cout << "Merkle root mismatch in block " << index
                      << " while loading " << filename << std::endl;
            return false;
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

// Single source of truth for the difficulty required of the block at
// `height`, replayed from the timing of the blocks below it. Mining
// (via calculateRequiredDifficulty) and validation use this same rule.
int Blockchain::calculateRequiredDifficultyAtHeight(int height) const {
    if (height < DIFFICULTY_ADJUSTMENT_INTERVAL) {
        return difficulty;
    }

    const auto& interval_start = chain[height - DIFFICULTY_ADJUSTMENT_INTERVAL];
    const auto& previous = chain[height - 1];

    long long expected = TARGET_BLOCK_TIME * DIFFICULTY_ADJUSTMENT_INTERVAL;
    long long actual = previous->getTimestamp() - interval_start->getTimestamp();
    int current = previous->getDifficulty();

    if (actual < expected / 2) {
        return current + 1;
    } else if (actual > expected * 2) {
        return std::max(1, current - 1);
    }

    return current;
}
