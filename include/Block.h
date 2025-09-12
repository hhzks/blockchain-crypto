#ifndef BLOCK_H
#define BLOCK_H

#include <string>
#include <vector>
#include <memory>
#include "Transaction.h"

/**
 * Represents a block in the blockchain
 */
class Block {
private:
    int index;                                    // Position in blockchain
    long long timestamp;                          // When block was created
    std::vector<std::shared_ptr<Transaction>> transactions; // Transactions in this block
    std::string previousHash;                     // Hash of previous block
    std::string merkleRoot;                       // Merkle root of transactions
    int nonce;                                    // Proof of work value
    std::string hash;                            // Hash of this block

public:
    /**
     * Constructor for a new block
     * @param idx Index of the block
     * @param prevHash Hash of the previous block
     */
    Block(int idx, const std::string& prevHash);
    
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
     * @param difficulty Number of leading zeros required in hash
     */
    void mineBlock(int difficulty);
    
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
     * Validate this block
     * @param difficulty Required difficulty for proof of work
     * @return True if block is valid, false otherwise
     */
    bool isValid(int difficulty) const;
    
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

#endif // BLOCK_H
