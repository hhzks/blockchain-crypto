#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "Block.h"
#include "Transaction.h"
#include "money.h"

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
    static constexpr money::Amount INITIAL_MINING_REWARD = money::coins(50);

    std::vector<std::shared_ptr<Block>> chain;
    std::vector<std::shared_ptr<Transaction>> pending_transactions;
    int difficulty;
    money::Amount mining_reward;
    std::unordered_map<std::string, money::Amount> balances;

    mutable std::recursive_mutex chain_mutex;

public:
    Blockchain();
    Blockchain(int initial_difficulty, money::Amount initial_reward);
    // Deleted for the same reason as Transaction's: 50.0 would quietly mean
    // 50 units rather than 50 coins.
    Blockchain(int initial_difficulty, double initial_reward) = delete;

    std::shared_ptr<Block> createGenesisBlock();
    std::shared_ptr<Block> getLatestBlock() const;
    void addTransaction(std::shared_ptr<Transaction> transaction);
    void minePendingTransactions(const std::string& reward_address);
    // Served from the balances cache, which updateBalances rebuilds on every
    // path that appends a block. The chain rescan this used to do was
    // O(blocks x transactions) on the hot path: addTransaction calls it for
    // every submitted transaction.
    money::Amount getBalance(const std::string& address) const;
    // Total already queued for spending by `address` in the pending pool.
    money::Amount pendingOutflow(const std::string& address) const;
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
    money::Amount getMiningReward() const {
        std::scoped_lock lock(chain_mutex);
        return mining_reward;
    }
    void setMiningReward(money::Amount reward) {
        std::scoped_lock lock(chain_mutex);
        mining_reward = reward;
    }
    void setMiningReward(double reward) = delete;
    size_t getChainSize() const {
        std::scoped_lock lock(chain_mutex);
        return chain.size();
    }

private:
    int calculateRequiredDifficultyAtHeight(int height) const;
};
