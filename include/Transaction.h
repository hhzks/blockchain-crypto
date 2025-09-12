#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <ctime>

/**
 * Represents a transaction in the blockchain
 */
class Transaction {
private:
    std::string sender;      // Address of sender
    std::string receiver;    // Address of receiver
    double amount;           // Amount being transferred
    long long timestamp;     // When transaction was created
    std::string signature;   // Digital signature (simplified)

public:
    /**
     * Constructor for a new transaction
     * @param from Sender address
     * @param to Receiver address
     * @param value Amount to transfer
     */
    Transaction(const std::string& from, const std::string& to, double value);
    
    /**
     * Get sender address
     * @return Sender address string
     */
    std::string getSender() const { return sender; }
    
    /**
     * Get receiver address
     * @return Receiver address string
     */
    std::string getReceiver() const { return receiver; }
    
    /**
     * Get transaction amount
     * @return Amount as double
     */
    double getAmount() const { return amount; }
    
    /**
     * Get transaction timestamp
     * @return Timestamp as long long
     */
    long long getTimestamp() const { return timestamp; }
    
    /**
     * Get transaction signature
     * @return Signature string
     */
    std::string getSignature() const { return signature; }
    
    /**
     * Calculate hash of this transaction
     * @return Transaction hash as string
     */
    std::string calculateHash() const;
    
    /**
     * Sign the transaction (simplified implementation)
     * @param privateKey Private key for signing (simplified)
     */
    void signTransaction(const std::string& privateKey);
    
    /**
     * Verify transaction signature (simplified implementation)
     * @param publicKey Public key for verification (simplified)
     * @return True if signature is valid, false otherwise
     */
    bool verifySignature(const std::string& publicKey) const;
    
    /**
     * Convert transaction to string representation
     * @return String representation of transaction
     */
    std::string toString() const;
    
    /**
     * Validate transaction (check amounts, addresses, etc.)
     * @return True if transaction is valid, false otherwise
     */
    bool isValid() const;
};

#endif // TRANSACTION_H
