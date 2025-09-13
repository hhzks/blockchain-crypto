#pragma once

#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

namespace utils {
    /**
     * Calculate SHA-256 hash of input string
     * @param input String to hash
     * @return Hexadecimal string representation of hash
     */
    std::string sha256(const std::string& input);
    
    /**
     * Calculate Merkle root from a vector of transaction hashes
     * @param transactions Vector of transaction hash strings
     * @return Merkle root hash as string
     */
    std::string calculateMerkleRoot(const std::vector<std::string>& transactions);
    
    /**
     * Get current timestamp in seconds since epoch
     * @return Current timestamp as long long
     */
    long long getCurrentTimestamp();
    
    /**
     * Convert bytes to hexadecimal string
     * @param data Vector of bytes
     * @return Hexadecimal string representation
     */
    std::string bytesToHex(const std::vector<unsigned char>& data);
    
    /**
     * Check if hash meets difficulty requirement (starts with required number of zeros)
     * @param hash Hash string to check
     * @param difficulty Number of leading zeros required
     * @return True if hash meets difficulty, false otherwise
     */
    bool checkProofOfWork(const std::string& hash, int difficulty);
}
