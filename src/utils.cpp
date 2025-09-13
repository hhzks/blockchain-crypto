#include "include/utils.h"
#include "include/sha.h"
#include <chrono>
#include <algorithm>

namespace utils {

std::string sha256(const std::string& input) {
    return SHA256::hash(input);
}

std::string calculateMerkleRoot(const std::vector<std::string>& transactions) {
    if (transactions.empty()) {
        return sha256(""); // Empty merkle root
    }
    
    if (transactions.size() == 1) {
        return transactions[0];
    }
    
    std::vector<std::string> currentLevel = transactions;
    
    while (currentLevel.size() > 1) {
        std::vector<std::string> nextLevel;
        
        for (size_t i = 0; i < currentLevel.size(); i += 2) {
            std::string combined;
            if (i + 1 < currentLevel.size()) {
                combined = currentLevel[i] + currentLevel[i + 1];
            } else {
                // If odd number of elements, duplicate the last one
                combined = currentLevel[i] + currentLevel[i];
            }
            nextLevel.push_back(sha256(combined));
        }
        
        currentLevel = nextLevel;
    }
    
    return currentLevel[0];
}

long long getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

std::string bytesToHex(const std::vector<unsigned char>& data) {
    std::stringstream ss;
    for (unsigned char byte : data) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
}

bool checkProofOfWork(const std::string& hash, int difficulty) {
    std::string target(difficulty, '0');
    return hash.substr(0, difficulty) == target;
}

} // namespace utils
