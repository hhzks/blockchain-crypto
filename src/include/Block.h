#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Transaction.h"

class Block {
private:
    int index;
    long long timestamp;
    std::vector<std::shared_ptr<Transaction>> transactions;
    std::string previous_hash;
    std::string merkle_root;
    int difficulty;
    int nonce;
    std::string hash;

public:
    Block(int idx, const std::string& prev_hash, int block_difficulty);
    // Restore constructor for deserialization: preserves the original
    // timestamp instead of stamping the current time.
    Block(int idx, const std::string& prev_hash, int block_difficulty,
          long long block_timestamp);

    void addTransaction(std::shared_ptr<Transaction> transaction);
    std::string calculateHash() const;
    void mineBlock();
    void calculateMerkleRoot();
    // Restore a previously mined block's nonce and hash after its
    // transactions have been re-added.
    void setMinedState(int mined_nonce, const std::string& mined_hash);

    int getIndex() const { return index; }
    long long getTimestamp() const { return timestamp; }
    std::string getPreviousHash() const { return previous_hash; }
    std::string getHash() const { return hash; }
    std::string getMerkleRoot() const { return merkle_root; }
    int getDifficulty() const { return difficulty; }
    int getNonce() const { return nonce; }
    const std::vector<std::shared_ptr<Transaction>>& getTransactions() const { return transactions; }

    bool isValid() const;
    bool isValidWithDifficulty(int required_difficulty) const;
    bool isMined(int difficulty) const;
    std::string toString() const;
};
