#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include "Block.h"
#include "Transaction.h"

/**
 * Main blockchain class that manages the chain of blocks
 */
class Blockchain {
private:
    std::vector<std::shared_ptr<Block>> chain;
    std::vector<std::shared_ptr<Transaction>> pendingTransactions;
    int difficulty;
    double miningReward;
    std::unordered_map<std::string, double> balances;

public:
    /**
     * Constructor - creates blockchain with genesis block
     */
    Blockchain();
    
    /**
     * Create the genesis block (first block in chain)
     * @return Shared pointer to genesis block
     */
    std::shared_ptr<Block> createGenesisBlock();
    
    /**
     * Get the latest block in the chain
     * @return Shared pointer to latest block
     */
    std::shared_ptr<Block> getLatestBlock() const;
    
    /**
     * Add a pending transaction to be included in next block
     * @param transaction Transaction to add
     */
    void addTransaction(std::shared_ptr<Transaction> transaction);
    
    /**
     * Mine a new block with pending transactions
     * @param miningRewardAddress Address to receive mining reward
     */
    void minePendingTransactions(const std::string& miningRewardAddress);
    
    /**
     * Get balance for a given address
     * @param address Address to check balance for
     * @return Balance as double
     */
    double getBalance(const std::string& address);
    
    /**
     * Validate the entire blockchain
     * @return True if blockchain is valid, false otherwise
     */
    bool isChainValid() const;
    
    /**
     * Get the entire blockchain
     * @return Vector of block pointers
     */
    const std::vector<std::shared_ptr<Block>>& getChain() const { return chain; }
    
    /**
     * Get current difficulty
     * @return Difficulty as integer
     */
    int getDifficulty() const { return difficulty; }
    
    /**
     * Set mining difficulty
     * @param newDifficulty New difficulty value
     */
    void setDifficulty(int newDifficulty) { difficulty = newDifficulty; }
    
    /**
     * Get mining reward amount
     * @return Mining reward as double
     */
    double getMiningReward() const { return miningReward; }
    
    /**
     * Set mining reward amount
     * @param reward New reward amount
     */
    void setMiningReward(double reward) { miningReward = reward; }
    
    /**
     * Get pending transactions
     * @return Vector of pending transaction pointers
     */
    const std::vector<std::shared_ptr<Transaction>>& getPendingTransactions() const { 
        return pendingTransactions; 
    }
    
    /**
     * Display the entire blockchain
     */
    void printChain() const;
    
    /**
     * Save blockchain to file
     * @param filename File to save to
     * @return True if saved successfully, false otherwise
     */
    bool saveToFile(const std::string& filename) const;
    
    /**
     * Load blockchain from file
     * @param filename File to load from
     * @return True if loaded successfully, false otherwise
     */
    bool loadFromFile(const std::string& filename);
    
    /**
     * Get total number of blocks in chain
     * @return Number of blocks
     */
    size_t getChainSize() const { return chain.size(); }
    
    /**
     * Update balances based on all transactions in the blockchain
     */
    void updateBalances();
    
    /**
     * Check if a transaction already exists in the blockchain
     * @param transaction Transaction to check
     * @return True if transaction exists, false otherwise
     */
    bool transactionExists(const std::shared_ptr<Transaction>& transaction) const;
};
