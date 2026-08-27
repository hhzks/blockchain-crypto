#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "Block.h"
#include "Transaction.h"

// Thread-safe. The P2P receiver thread calls addBlock/addTransaction through
// the node callbacks while the CLI thread mines, queries balances and saves,
// so every public method takes chain_mutex for its whole duration and the
// container accessors return snapshots rather than references into the live
// state.
//
// The mutex is recursive because the public methods compose -- addTransaction
// consults getBalance and transactionExists, minePendingTransactions consults
// getLatestBlock and updateBalances -- and a plain mutex would self-deadlock
// on the first such call. Note that minePendingTransactions holds the lock
// while it mines, which serialises an arriving block behind local mining;
// that is acceptable at the difficulties this project uses.
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

    mutable std::recursive_mutex chain_mutex;

public:
    Blockchain();
    Blockchain(int initial_difficulty, double initial_reward);

    std::shared_ptr<Block> createGenesisBlock();
    std::shared_ptr<Block> getLatestBlock() const;
    void addTransaction(std::shared_ptr<Transaction> transaction);
    void minePendingTransactions(const std::string& reward_address);
    double getBalance(const std::string& address);
    // Total already queued for spending by `address` in the pending pool.
    double pendingOutflow(const std::string& address) const;
    int calculateRequiredDifficulty() const;
    bool validateBlockDifficulty(const std::shared_ptr<Block>& block) const;
    bool isChainValid() const;
    bool addBlock(std::shared_ptr<Block> block);
    void printChain() const;
    bool saveToFile(const std::string& filename) const;
    bool loadFromFile(const std::string& filename);
    void updateBalances();
    bool transactionExists(const std::shared_ptr<Transaction>& transaction) const;

    // Snapshots: returning a reference would hand callers an alias into a
    // container the peer thread resizes.
    std::vector<std::shared_ptr<Block>> getChain() const {
        std::scoped_lock lock(chain_mutex);
        return chain;
    }
    std::vector<std::shared_ptr<Transaction>> getPendingTransactions() const {
        std::scoped_lock lock(chain_mutex);
        return pending_transactions;
    }
    int getDifficulty() const {
        std::scoped_lock lock(chain_mutex);
        return difficulty;
    }
    double getMiningReward() const {
        std::scoped_lock lock(chain_mutex);
        return mining_reward;
    }
    void setMiningReward(double reward) {
        std::scoped_lock lock(chain_mutex);
        mining_reward = reward;
    }
    size_t getChainSize() const {
        std::scoped_lock lock(chain_mutex);
        return chain.size();
    }

private:
    int calculateRequiredDifficultyAtHeight(int height) const;
};
