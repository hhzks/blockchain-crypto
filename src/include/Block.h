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
    std::string previousHash;
    std::string merkleRoot;
    int difficulty;
    int nonce;
    std::string hash;
public:
    /**
     * Constructor for a new block
     * @param idx Index of the block
     * @param prevHash Hash of the previous block
     * @param blockDifficulty Difficulty this block must be mined with
     */
    Block(int idx, const std::string& prevHash, int blockDifficulty);
    
    /**
     * Add a transaction to this block
     * @param transaction Transaction to add
     */
    void addTransaction(std::shared_ptr<Transaction> transaction);
    
    /**
     * Calculate hash of this block
     * @return Block hash as string
     */
    std::string calculateHash() const;
    
    /**
     * Mine this block using proof of work
     */
    void mineBlock();
    
    /**
     * Get block index
     * @return Block index
     */
    int getIndex() const { return index; }
    
    /**
     * Get block timestamp
     * @return Timestamp as long long
     */
    long long getTimestamp() const { return timestamp; }
    
    /**
     * Get previous block hash
     * @return Previous hash string
     */
    std::string getPreviousHash() const { return previousHash; }
    
    /**
     * Get block hash
     * @return Block hash string
     */
    std::string getHash() const { return hash; }
    
    /**
     * Get merkle root
     * @return Merkle root string
     */
    std::string getMerkleRoot() const { return merkleRoot; }

    /**
     * Get difficulty of block
     * @return difficulty
     */
    int getDifficulty() const { return difficulty; }
    
    /**
     * Get nonce value
     * @return Nonce as integer
     */
    int getNonce() const { return nonce; }
    
    /**
     * Get transactions in this block
     * @return Vector of transaction pointers
     */
    const std::vector<std::shared_ptr<Transaction>>& getTransactions() const { return transactions; }
    
    /**
     * Validate this block (uses stored difficulty)
     * @return True if block is valid, false otherwise
     */
    bool isValid() const;
    
    /**
     * Validate this block with required difficulty
     * @param requiredDifficulty Required difficulty for validation
     * @return True if block is valid, false otherwise
     */
    bool isValidWithDifficulty(int requiredDifficulty) const;
    
    /**
     * Convert block to string representation
     * @return String representation of block
     */
    std::string toString() const;
    
    /**
     * Calculate merkle root for all transactions in this block
     */
    void calculateMerkleRoot();
    
    /**
     * Check if block has been mined (hash meets difficulty)
     * @param difficulty Required difficulty
     * @return True if mined, false otherwise
     */
    bool isMined(int difficulty) const;
};
