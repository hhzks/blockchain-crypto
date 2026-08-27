#include "include/Blockchain.h"
#include "include/utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <unordered_set>
#include <format>
#include <cmath>

Blockchain::Blockchain() : difficulty(INITIAL_DIFFICULTY), mining_reward(INITIAL_MINING_REWARD) {
    chain.push_back(createGenesisBlock());
    updateBalances();
}

Blockchain::Blockchain(int initial_difficulty, money::Amount initial_reward)
    : difficulty(initial_difficulty), mining_reward(initial_reward) {
    chain.push_back(createGenesisBlock());
    updateBalances();
}

std::shared_ptr<Block> Blockchain::createGenesisBlock() {
    std::scoped_lock lock(chain_mutex);

    // The genesis block carries no transactions: a zero-amount marker tx
    // would be rejected by Block::addTransaction's validity check anyway.
    auto genesis = std::make_shared<Block>(0, "0", difficulty);
    genesis->mineBlock();
    std::cout << "Genesis block created!" << std::endl;
    return genesis;
}

std::shared_ptr<Block> Blockchain::getLatestBlock() const {
    std::scoped_lock lock(chain_mutex);

    return chain.back();
}

void Blockchain::addTransaction(std::shared_ptr<Transaction> transaction) {
    std::scoped_lock lock(chain_mutex);

    if (!transaction->isValid()) {
        std::cout << "Invalid transaction rejected!" << std::endl;
        return;
    }
    
    if (transactionExists(transaction)) {
        std::cout << "Transaction already exists in blockchain!" << std::endl;
        return;
    }
    
    if (transaction->getSender() != "system") {
        // getBalance only sums mined history, so without the pending outflow a
        // sender could queue the same funds any number of times and
        // minePendingTransactions would pack every copy into one block.
        money::Amount balance = getBalance(transaction->getSender());
        money::Amount pending_outflow = pendingOutflow(transaction->getSender());
        money::Amount available = balance - pending_outflow;

        if (available < transaction->getAmount()) {
            std::cout << "Transaction rejected: Insufficient balance. Balance: "
                      << money::format(balance) << ", already pending: "
                      << money::format(pending_outflow) << ", Required: "
                      << money::format(transaction->getAmount()) << std::endl;
            return;
        }
    }
    
    pending_transactions.push_back(transaction);
    std::cout << "Transaction added to pending pool" << std::endl;
}

void Blockchain::minePendingTransactions(const std::string& reward_address) {
    std::scoped_lock lock(chain_mutex);

    // An empty pool is mined anyway, for the reward alone. Refusing left a
    // fresh chain with no way to mint a first reward, so no address could ever
    // be funded and no transaction could ever be afforded.
    if (pending_transactions.empty()) {
        std::cout << "No pending transactions; mining for the reward only."
                  << std::endl;
    }
    
    int required_difficulty = calculateRequiredDifficulty();
    
    std::cout << "Starting to mine block with " << pending_transactions.size() 
              << " transactions at difficulty " << required_difficulty << "..." << std::endl;
    
    auto new_block = std::make_shared<Block>(
        static_cast<int>(chain.size()),
        getLatestBlock()->getHash(),
        required_difficulty
    );
    
    for (auto& tx : pending_transactions) {
        // A rejection is reported by Block::addTransaction; the transaction is
        // left out of the block and dropped with the rest of the pool below.
        new_block->addTransaction(tx);
    }
    
    auto reward_tx = std::make_shared<Transaction>("system", reward_address, mining_reward);
    if (!new_block->addTransaction(reward_tx)) {
        std::cout << "Mining aborted: invalid mining reward transaction of "
                  << money::format(mining_reward) << " to '" << reward_address
                  << "'. Pending transactions kept." << std::endl;
        return;
    }
    
    new_block->mineBlock();
    
    if (!validateBlockDifficulty(new_block)) {
        std::cout << "CRITICAL ERROR: Mined block has incorrect difficulty!" << std::endl;
        return;
    }
    
    chain.push_back(new_block);
    pending_transactions.clear();
    updateBalances();
    
    std::cout << "Block mined and added to blockchain!" << std::endl;
}

int Blockchain::calculateRequiredDifficulty() const {
    std::scoped_lock lock(chain_mutex);

    return calculateRequiredDifficultyAtHeight(static_cast<int>(chain.size()));
}

bool Blockchain::validateBlockDifficulty(const std::shared_ptr<Block>& block) const {
    std::scoped_lock lock(chain_mutex);

    int required = calculateRequiredDifficultyAtHeight(block->getIndex());
    int actual = block->getDifficulty();
    
    if (actual != required) {
        std::cout << "Block " << block->getIndex() 
                  << " has incorrect difficulty: " << actual 
                  << " (required: " << required << ")" << std::endl;
        return false;
    }
    
    return true;
}

money::Amount Blockchain::getBalance(const std::string& address) const {
    std::scoped_lock lock(chain_mutex);

    auto it = balances.find(address);
    return it != balances.end() ? it->second : money::Amount{0};
}

money::Amount Blockchain::pendingOutflow(const std::string& address) const {
    std::scoped_lock lock(chain_mutex);

    money::Amount outflow = 0;

    for (const auto& transaction : pending_transactions) {
        if (transaction->getSender() == address) {
            outflow += transaction->getAmount();
        }
    }

    return outflow;
}

bool Blockchain::isChainValid() const {
    std::scoped_lock lock(chain_mutex);

    if (chain.size() < 1) {
        return false;
    }
    
    // Validate genesis block
    if (!chain[0]->isValidWithDifficulty(difficulty)) {
        std::cout << "Invalid genesis block" << std::endl;
        return false;
    }
    
    // Check each block
    for (size_t i = 1; i < chain.size(); i++) {
        const auto& currentBlock = chain[i];
        const auto& previousBlock = chain[i - 1];
        
        // Calculate what the difficulty should be at this height
        int requiredDifficulty = calculateRequiredDifficultyAtHeight(i);
        
        // Validate current block with required difficulty
        if (!currentBlock->isValidWithDifficulty(requiredDifficulty)) {
            std::cout << "Invalid block found at index " << i << std::endl;
            return false;
        }
        
        // Check if current block's previous hash matches previous block's hash
        if (currentBlock->getPreviousHash() != previousBlock->getHash()) {
            std::cout << "Chain broken at block " << i << std::endl;
            return false;
        }
        
        // Verify timestamp is reasonable (prevent time manipulation).
        // Equal timestamps are allowed: timestamps have second granularity
        // and consecutive blocks can be mined within the same second.
        if (currentBlock->getTimestamp() < previousBlock->getTimestamp()) {
            std::cout << "Block " << i << " has invalid timestamp" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool Blockchain::addBlock(std::shared_ptr<Block> block) {
    std::scoped_lock lock(chain_mutex);

    if (!block) {
        return false;
    }

    const auto& tip = getLatestBlock();

    // Must extend the current tip at exactly the next height.
    if (block->getIndex() != static_cast<int>(chain.size())) {
        std::cout << "Rejected block: wrong index " << block->getIndex()
                  << " (expected " << chain.size() << ")" << std::endl;
        return false;
    }

    if (block->getPreviousHash() != tip->getHash()) {
        std::cout << "Rejected block: previous hash does not match tip" << std::endl;
        return false;
    }

    // Second-granularity timestamps; equal is allowed (matches isChainValid).
    if (block->getTimestamp() < tip->getTimestamp()) {
        std::cout << "Rejected block: timestamp precedes tip" << std::endl;
        return false;
    }

    int required = calculateRequiredDifficultyAtHeight(block->getIndex());
    if (!block->isValidWithDifficulty(required)) {
        std::cout << "Rejected block: failed validation" << std::endl;
        return false;
    }

    // A transaction already mined into this chain must not be replayed: its
    // signature still verifies and the proof of work is real, so nothing else
    // in the validation path stops a peer from crediting the receiver twice.
    // System rewards are exempt because two blocks may legitimately pay the
    // same miner the same amount in the same second, which is indistinguishable
    // by hash while transactions carry no nonce (#27).
    for (const auto& tx : block->getTransactions()) {
        if (tx->getSender() != "system" && transactionExists(tx)) {
            std::cout << "Rejected block: contains transaction already in the "
                         "chain (" << tx->getSender() << " -> "
                      << tx->getReceiver() << ")" << std::endl;
            return false;
        }
    }

    // Reward invariant: a legitimate block carries exactly one system
    // (mining-reward) transaction of the expected amount, matching how
    // minePendingTransactions builds blocks. Without this, a peer could mint
    // unlimited coins in a single crafted block, since system transactions are
    // exempt from signature verification and per-transaction validation alone
    // never bounds their count or amount.
    int system_tx_count = 0;
    for (const auto& tx : block->getTransactions()) {
        if (tx->getSender() == "system") {
            system_tx_count++;
            if (tx->getAmount() != mining_reward) {
                std::cout << "Rejected block: system reward amount "
                          << money::format(tx->getAmount())
                          << " does not match expected "
                          << money::format(mining_reward) << std::endl;
                return false;
            }
        }
    }
    if (system_tx_count != 1) {
        std::cout << "Rejected block: expected exactly one system reward "
                     "transaction, found " << system_tx_count << std::endl;
        return false;
    }

    chain.push_back(block);

    // Drop any pending transactions now included in the accepted block.
    if (!pending_transactions.empty()) {
        std::unordered_set<std::string> included;
        for (const auto& tx : block->getTransactions()) {
            included.insert(tx->calculateHash());
        }
        std::erase_if(pending_transactions,
                      [&included](const std::shared_ptr<Transaction>& tx) {
                          return included.contains(tx->calculateHash());
                      });
    }

    updateBalances();
    std::cout << "Block " << block->getIndex()
              << " accepted and added to chain" << std::endl;
    return true;
}

void Blockchain::printChain() const {
    std::scoped_lock lock(chain_mutex);

    std::cout << "=== BLOCKCHAIN ===" << std::endl;
    std::cout << "Chain length: " << chain.size() << " blocks" << std::endl;
    std::cout << "Difficulty: " << difficulty << std::endl;
    std::cout << "Mining reward: " << money::format(mining_reward) << std::endl;
    std::cout << "Pending transactions: " << pending_transactions.size() << std::endl;
    std::cout << std::endl;
    
    for (const auto& block : chain) {
        std::cout << block->toString() << std::endl;
    }
    
    if (!pending_transactions.empty()) {
        std::cout << "=== PENDING TRANSACTIONS ===" << std::endl;
        for (size_t i = 0; i < pending_transactions.size(); i++) {
            std::cout << (i + 1) << ". " << pending_transactions[i]->toString() << std::endl;
        }
    }
}

bool Blockchain::saveToFile(const std::string& filename) const {
    std::scoped_lock lock(chain_mutex);

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cout << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }

    file << difficulty << std::endl;
    file << mining_reward << std::endl;
    file << chain.size() << std::endl;

    // Save each block
    for (const auto& block : chain) {
        file << block->getIndex() << std::endl;
        file << block->getTimestamp() << std::endl;
        file << block->getPreviousHash() << std::endl;
        file << block->getHash() << std::endl;
        file << block->getMerkleRoot() << std::endl;
        file << block->getDifficulty() << std::endl;
        file << block->getNonce() << std::endl;

        const auto& transactions = block->getTransactions();
        file << transactions.size() << std::endl;

        for (const auto& tx : transactions) {
            file << tx->getSender() << std::endl;
            file << tx->getReceiver() << std::endl;
            // Fixed precision matching Transaction::calculateHash so the
            // reloaded amount reproduces the same hash.
            file << tx->getAmount() << std::endl;
            file << tx->getTimestamp() << std::endl;
            // "-" sentinel: an empty signature line would be skipped by
            // operator>> on load and corrupt the parse.
            file << (tx->getSignature().empty() ? "-" : tx->getSignature()) << std::endl;
            file << (tx->getSenderPublicKey().empty() ? "-" : tx->getSenderPublicKey()) << std::endl;
            file << tx->getNonce() << std::endl;
        }
    }

    file.close();
    std::cout << "Blockchain saved to " << filename << std::endl;
    return true;
}

bool Blockchain::loadFromFile(const std::string& filename) {
    std::scoped_lock lock(chain_mutex);

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }

    // Every extraction below is checked. Once a stream is in a fail state,
    // operator>> returns without touching its target, so an unchecked read
    // leaves the variable at whatever it held before -- which is how a
    // malformed file used to drive the block loop with an indeterminate count.
    auto fail = [&filename](const std::string& reason) {
        std::cout << "Failed to load " << filename << ": " << reason << std::endl;
        return false;
    };

    int file_difficulty = 0;
    money::Amount file_reward = 0;
    size_t chain_size = 0;

    if (!(file >> file_difficulty >> file_reward)) {
        return fail("unreadable header (difficulty, mining reward)");
    }
    if (!money::isValidAmount(file_reward)) {
        return fail(std::format("invalid mining reward {}", file_reward));
    }
    if (!(file >> chain_size)) {
        return fail("unreadable block count");
    }

    // Parse into a local chain: the live one stays untouched until the file
    // has been read in full and validated, so a typo'd filename or a corrupt
    // file no longer leaves the node with nothing.
    std::vector<std::shared_ptr<Block>> parsed;

    for (size_t i = 0; i < chain_size; i++) {
        int index = 0;
        long long timestamp = 0;
        std::string prev_hash, hash, merkle_root;
        int block_difficulty = 0;
        int nonce = 0;

        if (!(file >> index >> timestamp >> prev_hash >> hash >> merkle_root
                   >> block_difficulty >> nonce)) {
            return fail(std::format("unreadable header for block {}", i));
        }

        auto block = std::make_shared<Block>(index, prev_hash, block_difficulty, timestamp);

        size_t tx_count = 0;
        if (!(file >> tx_count)) {
            return fail(std::format("unreadable transaction count in block {}", i));
        }

        std::vector<std::shared_ptr<Transaction>> txs;

        for (size_t j = 0; j < tx_count; j++) {
            std::string sender, receiver, signature, pubkey;
            money::Amount amount = 0;
            long long tx_timestamp = 0;
            std::uint64_t tx_nonce = 0;

            if (!(file >> sender >> receiver >> amount >> tx_timestamp
                       >> signature >> pubkey >> tx_nonce)) {
                return fail(std::format("unreadable transaction {} in block {}", j, i));
            }
            if (signature == "-") {
                signature.clear();
            }
            if (pubkey == "-") {
                pubkey.clear();
            }

            auto tx = std::make_shared<Transaction>(sender, receiver, amount,
                                                    tx_timestamp, signature, tx_nonce);
            tx->setSenderPublicKey(pubkey);
            txs.push_back(tx);
        }

        // Restore the block as written, then check it; rebuilding it through
        // addTransaction would drop invalid transactions and report the
        // resulting merkle mismatch instead of the real defect.
        block->setTransactions(std::move(txs));
        block->setMinedState(nonce, hash);

        if (block->getMerkleRoot() != merkle_root) {
            return fail(std::format("merkle root mismatch in block {}", index));
        }

        parsed.push_back(block);
    }

    file.close();

    // Adopt the parsed chain only if it validates: hashes, proof of work,
    // difficulty schedule, previous-hash linkage and timestamps were all
    // previously taken from the file on trust.
    auto previous_chain = std::move(chain);
    const int previous_difficulty = difficulty;
    const double previous_reward = mining_reward;

    chain = std::move(parsed);
    difficulty = file_difficulty;
    mining_reward = file_reward;

    if (!isChainValid()) {
        chain = std::move(previous_chain);
        difficulty = previous_difficulty;
        mining_reward = previous_reward;
        return fail("chain failed validation");
    }

    pending_transactions.clear();
    updateBalances();

    std::cout << "Blockchain loaded from " << filename << std::endl;
    return true;
}

void Blockchain::updateBalances() {
    std::scoped_lock lock(chain_mutex);

    balances.clear();
    
    for (const auto& block : chain) {
        for (const auto& transaction : block->getTransactions()) {
            // Credit receiver
            balances[transaction->getReceiver()] += transaction->getAmount();
            
            // "system" is the mint, not an account: a mining reward has no
            // funding source to debit. This is now the only definition of the
            // rule -- getBalance used to carry a second, subtly different one
            // that debited "system" too, and the two could only agree while
            // nothing read this map.
            if (transaction->getSender() != "system") {
                balances[transaction->getSender()] -= transaction->getAmount();
            }
        }
    }
}

bool Blockchain::transactionExists(const std::shared_ptr<Transaction>& tx) const {
    std::scoped_lock lock(chain_mutex);

    std::string tx_hash = tx->calculateHash();
    
    for (const auto& block : chain) {
        for (const auto& existing : block->getTransactions()) {
            if (existing->calculateHash() == tx_hash) {
                return true;
            }
        }
    }
    
    for (const auto& pending : pending_transactions) {
        if (pending->calculateHash() == tx_hash) {
            return true;
        }
    }
    
    return false;
}

// Single source of truth for the difficulty required of the block at
// `height`, replayed from the timing of the blocks below it. Mining
// (via calculateRequiredDifficulty) and validation use this same rule.
int Blockchain::calculateRequiredDifficultyAtHeight(int height) const {
    std::scoped_lock lock(chain_mutex);

    if (height < DIFFICULTY_ADJUSTMENT_INTERVAL) {
        return difficulty;
    }

    const auto& interval_start = chain[height - DIFFICULTY_ADJUSTMENT_INTERVAL];
    const auto& previous = chain[height - 1];

    long long expected = TARGET_BLOCK_TIME * DIFFICULTY_ADJUSTMENT_INTERVAL;
    long long actual = previous->getTimestamp() - interval_start->getTimestamp();
    int current = previous->getDifficulty();

    if (actual < expected / 2) {
        return current + 1;
    } else if (actual > expected * 2) {
        return std::max(1, current - 1);
    }

    return current;
}
