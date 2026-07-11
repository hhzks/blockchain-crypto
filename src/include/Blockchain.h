#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include "Block.h"
#include "Transaction.h"

class Blockchain {
private:
    static constexpr int DIFFICULTY_ADJUSTMENT_INTERVAL = 10;
    // Seconds, matching the second-granularity block timestamps.
    static constexpr long long TARGET_BLOCK_TIME = 30;
    static constexpr int INITIAL_DIFFICULTY = 2;
    static constexpr int INITIAL_MINING_REWARD = 50;

    std::vector<std::shared_ptr<Block>> chain;
    std::vector<std::shared_ptr<Transaction>> pending_transactions;
    int difficulty;
    double mining_reward;
    std::unordered_map<std::string, double> balances;

public:
    Blockchain();
    Blockchain(int initial_difficulty, double initial_reward);

    std::shared_ptr<Block> createGenesisBlock();
    std::shared_ptr<Block> getLatestBlock() const;
    void addTransaction(std::shared_ptr<Transaction> transaction);
    void minePendingTransactions(const std::string& reward_address);
    double getBalance(const std::string& address);
    int calculateRequiredDifficulty() const;
    bool validateBlockDifficulty(const std::shared_ptr<Block>& block) const;
    bool isChainValid() const;
    void printChain() const;
    bool saveToFile(const std::string& filename) const;
    bool loadFromFile(const std::string& filename);
    void updateBalances();
    bool transactionExists(const std::shared_ptr<Transaction>& transaction) const;

    const std::vector<std::shared_ptr<Block>>& getChain() const { return chain; }
    int getDifficulty() const { return difficulty; }
    double getMiningReward() const { return mining_reward; }
    void setMiningReward(double reward) { mining_reward = reward; }
    size_t getChainSize() const { return chain.size(); }

    const std::vector<std::shared_ptr<Transaction>>& getPendingTransactions() const {
        return pending_transactions;
    }

private:
    int calculateRequiredDifficultyAtHeight(int height) const;
};
