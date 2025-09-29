#pragma once

#include <string>
#include <ctime>
#include "ECCrypto.h"

/**
 * Represents a transaction in the blockchain
 */
class Transaction {
private:
    std::string sender;      
    std::string receiver;
    double amount;      
    long long timestamp;
    std::string signature;   

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
     * Sign the transaction using ECC private key
     * @param privateKey ECC private key for signing
     * @return True if signing successful, false otherwise
     */
    bool signTransaction(const ECCrypto::PrivateKey& privateKey);
    
    /**
     * Sign the transaction using hex-encoded private key
     * @param privateKeyHex Hex string of private key
     * @return True if signing successful, false otherwise
     */
    bool signTransaction(const std::string& privateKeyHex);
    
    /**
     * Verify transaction signature using ECC public key
     * @param publicKey ECC public key for verification
     * @return True if signature is valid, false otherwise
     */
    bool verifySignature(const ECCrypto::PublicKey& publicKey) const;
    
    /**
     * Verify transaction signature using hex-encoded public key
     * @param publicKeyHex Hex string of public key
     * @return True if signature is valid, false otherwise
     */
    bool verifySignature(const std::string& publicKeyHex) const;
    
    /**
     * Verify transaction signature using address (derived from public key)
     * @param address Blockchain address to verify against
     * @return True if signature is valid, false otherwise
     */
    bool verifySignatureByAddress(const std::string& address) const;
    
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
    
private:
    /**
     * Get the canonical transaction data for signing/verification
     * @return Transaction data string
     */
    std::string getTransactionData() const;
};
