#include "include/Blockchain.h"
#include "include/utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>

Blockchain::Blockchain() : difficulty(4), miningReward(100.0) {
    // Create and add genesis block
    chain.push_back(createGenesisBlock());
}

std::shared_ptr<Block> Blockchain::createGenesisBlock() {
    auto genesisBlock = std::make_shared<Block>(0, "0");
    
    // Add a genesis transaction
    auto genesisTransaction = std::make_shared<Transaction>("system", "genesis", 0);
    genesisBlock->addTransaction(genesisTransaction);
    
    // Mine the genesis block
    genesisBlock->mineBlock(difficulty);
    
    std::cout << "Genesis block created!" << std::endl;
    return genesisBlock;
}

std::shared_ptr<Block> Blockchain::getLatestBlock() const {
    return chain.back();
}

void Blockchain::addTransaction(std::shared_ptr<Transaction> transaction) {
    if (!transaction->isValid()) {
        std::cout << "Invalid transaction rejected!" << std::endl;

        return;
    }
    
    // Check if transaction already exists
    if (transactionExists(transaction)) {
        std::cout << "Transaction already exists in blockchain!" << std::endl;
        return;
    }
    
    // For non-mining transactions, check if sender has sufficient balance
    if (transaction->getSender() != "system") {
        double senderBalance = getBalance(transaction->getSender());
        if (senderBalance < transaction->getAmount()) {
            std::cout << "Transaction rejected: Insufficient balance. Balance: " 
                      << senderBalance << ", Required: " << transaction->getAmount() << std::endl;
            return;
        }
    }
    
    pendingTransactions.push_back(transaction);
    std::cout << "Transaction added to pending pool" << std::endl;
}

void Blockchain::minePendingTransactions(const std::string& miningRewardAddress) {
    if (pendingTransactions.empty()) {
        std::cout << "No pending transactions to mine!" << std::endl;
        return;
    }
    
    std::cout << "Starting to mine block with " << pendingTransactions.size() << " transactions..." << std::endl;
    
    // Create new block
    auto newBlock = std::make_shared<Block>(
        static_cast<int>(chain.size()),
        getLatestBlock()->getHash()
    );
    
    // Add all pending transactions to the block
    for (auto& transaction : pendingTransactions) {
        newBlock->addTransaction(transaction);
    }
    
    // Add mining reward transaction
    auto rewardTransaction = std::make_shared<Transaction>("system", miningRewardAddress, miningReward);
    newBlock->addTransaction(rewardTransaction);
    
    // Mine the block
    newBlock->mineBlock(difficulty);
    
    // Add block to chain
    chain.push_back(newBlock);
    
    // Clear pending transactions
    pendingTransactions.clear();
    
    // Update balances
    updateBalances();
    
    std::cout << "Block mined and added to blockchain!" << std::endl;
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
    
    // Check each block
    for (size_t i = 1; i < chain.size(); i++) {
        const auto& currentBlock = chain[i];
        const auto& previousBlock = chain[i - 1];
        
        // Validate current block
        if (!currentBlock->isValid(difficulty)) {
            std::cout << "Invalid block found at index " << i << std::endl;
            return false;
        }
        
        // Check if current block's previous hash matches previous block's hash
        if (currentBlock->getPreviousHash() != previousBlock->getHash()) {
            std::cout << "Chain broken at block " << i << std::endl;
            return false;
        }
    }
    
    // Validate genesis block separately
    if (!chain[0]->isValid(difficulty)) {
        std::cout << "Invalid genesis block" << std::endl;
        return false;
    }
    
    return true;
}

void Blockchain::printChain() const {
    std::cout << "=== BLOCKCHAIN ===" << std::endl;
    std::cout << "Chain length: " << chain.size() << " blocks" << std::endl;
    std::cout << "Difficulty: " << difficulty << std::endl;
    std::cout << "Mining reward: " << miningReward << std::endl;
    std::cout << "Pending transactions: " << pendingTransactions.size() << std::endl;
    std::cout << std::endl;
    
    for (const auto& block : chain) {
        std::cout << block->toString() << std::endl;
    }
    
    if (!pendingTransactions.empty()) {
        std::cout << "=== PENDING TRANSACTIONS ===" << std::endl;
        for (size_t i = 0; i < pendingTransactions.size(); i++) {
            std::cout << (i + 1) << ". " << pendingTransactions[i]->toString() << std::endl;
        }
    }
}

bool Blockchain::saveToFile(const std::string& filename) const {
    std::ofstream file("blockchain_saves/" + filename);
    if (!file.is_open()) {
        std::cout << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }
    
    // Save blockchain metadata
    file << difficulty << std::endl;
    file << miningReward << std::endl;
    file << chain.size() << std::endl;
    
    // Save each block
    for (const auto& block : chain) {
        file << block->getIndex() << std::endl;
        file << block->getTimestamp() << std::endl;
        file << block->getPreviousHash() << std::endl;
        file << block->getHash() << std::endl;
        file << block->getMerkleRoot() << std::endl;
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
    
    // Clear current chain
    chain.clear();
    pendingTransactions.clear();
    
    // Load blockchain metadata
    file >> difficulty >> miningReward;
    
    size_t chainSize;
    file >> chainSize;
    
    // Load each block
    for (size_t i = 0; i < chainSize; i++) {
        int index;
        long long timestamp;
        std::string prevHash, hash, merkleRoot;
        int nonce;
        
        file >> index >> timestamp >> prevHash >> hash >> merkleRoot >> nonce;
        
        auto block = std::make_shared<Block>(index, prevHash);
        
        size_t txCount;
        file >> txCount;
        
        for (size_t j = 0; j < txCount; j++) {
            std::string sender, receiver, signature;
            double amount;
            long long txTimestamp;
            
            file >> sender >> receiver >> amount >> txTimestamp >> signature;
            
            auto transaction = std::make_shared<Transaction>(sender, receiver, amount);
            // Note: In a more complete implementation, we'd properly restore the timestamp and signature
            block->addTransaction(transaction);
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

bool Blockchain::transactionExists(const std::shared_ptr<Transaction>& transaction) const {
    std::string txHash = transaction->calculateHash();
    
    // Check in existing blocks
    for (const auto& block : chain) {
        for (const auto& existingTx : block->getTransactions()) {
            if (existingTx->calculateHash() == txHash) {
                return true;
            }
        }
    }
    
    // Check in pending transactions
    for (const auto& pendingTx : pendingTransactions) {
        if (pendingTx->calculateHash() == txHash) {
            return true;
        }
    }
    
    return false;
}
