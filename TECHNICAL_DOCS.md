# Blockchain from Scratch - Technical Documentation

## Overview

This is a complete blockchain implementation written in C++ that demonstrates all the fundamental concepts of blockchain technology. The implementation includes:

- Cryptographic hashing (SHA-256)
- Proof of Work consensus mechanism
- Transaction system with digital signatures
- Block structure with Merkle trees
- Chain validation
- Persistence (save/load)

## Architecture

### Core Components

#### 1. Transaction Class (`Transaction.h/cpp`)
Represents a single transaction in the blockchain.

**Key Features:**
- Sender and receiver addresses
- Transaction amount and timestamp
- Digital signature support (simplified)
- Hash calculation using SHA-256
- Transaction validation

**Methods:**
```cpp
Transaction(sender, receiver, amount)  // Constructor
calculateHash()                       // SHA-256 hash of transaction
signTransaction(privateKey)           // Sign with private key
verifySignature(publicKey)            // Verify signature
isValid()                            // Validate transaction rules
```

#### 2. Block Class (`Block.h/cpp`)
Represents a single block in the blockchain.

**Key Features:**
- Block index and timestamp
- Previous block hash (chain linking)
- Merkle root of all transactions
- Proof of Work (nonce)
- Transaction storage

**Methods:**
```cpp
Block(index, previousHash)            // Constructor
addTransaction(transaction)           // Add transaction to block
calculateHash()                      // Calculate block hash
mineBlock(difficulty)                // Perform Proof of Work
isValid(difficulty)                  // Validate block
```

#### 3. Blockchain Class (`Blockchain.h/cpp`)
Main blockchain implementation managing the entire chain.

**Key Features:**
- Genesis block creation
- Chain management
- Mining with rewards
- Balance tracking
- Chain validation
- Persistence

**Methods:**
```cpp
Blockchain()                         // Creates genesis block
addTransaction(transaction)          // Add to pending pool
minePendingTransactions(minerAddr)   // Mine new block
getBalance(address)                  // Calculate balance
isChainValid()                       // Validate entire chain
```

#### 4. Utilities (`utils.h/cpp`)
Cryptographic and utility functions.

**Functions:**
```cpp
sha256(input)                        // SHA-256 hashing
calculateMerkleRoot(transactions)    // Merkle tree root
getCurrentTimestamp()                // Current time
checkProofOfWork(hash, difficulty)   // Validate PoW
```

## Cryptographic Security

### SHA-256 Hashing
Every transaction and block uses SHA-256 hashing:
- **Transaction Hash**: Hash of sender + receiver + amount + timestamp
- **Block Hash**: Hash of index + timestamp + previous hash + merkle root + nonce + transactions
- **Merkle Root**: Binary tree hash of all transactions in a block

### Proof of Work
Mining algorithm that requires computational work:
1. Calculate block hash
2. Check if hash starts with required number of zeros (difficulty)
3. If not, increment nonce and repeat
4. Higher difficulty = more zeros required = more computation

### Digital Signatures (Simplified)
Basic signature system for transaction authorization:
- Signing: Hash(transaction_data + private_key)
- Verification: Compare with expected signature from public key

## Mining Process

### 1. Transaction Pool
- Transactions are added to pending pool
- Validated before acceptance
- Balance checks performed

### 2. Block Creation
- New block created with pending transactions
- Previous block hash linked
- Merkle root calculated

### 3. Mining (Proof of Work)
- Miner tries different nonce values
- Calculates hash until difficulty requirement met
- First to find valid hash wins block reward

### 4. Block Addition
- Valid block added to chain
- Pending transactions cleared
- Balances updated
- Mining reward distributed

## Data Structures

### Block Structure
```
Block {
    index: int
    timestamp: long long
    transactions: vector<Transaction>
    previousHash: string
    merkleRoot: string
    nonce: int
    hash: string
}
```

### Transaction Structure
```
Transaction {
    sender: string
    receiver: string
    amount: double
    timestamp: long long
    signature: string
}
```

### Blockchain Structure
```
Blockchain {
    chain: vector<Block>
    pendingTransactions: vector<Transaction>
    difficulty: int
    miningReward: double
    balances: map<string, double>
}
```

## Security Features

### 1. Immutability
- Each block contains hash of previous block
- Changing any transaction requires recalculating all subsequent blocks
- Computationally infeasible for large chains

### 2. Integrity Validation
- Hash verification for all blocks and transactions
- Merkle root validation ensures transaction integrity
- Chain linkage verification

### 3. Double Spending Prevention
- Balance checking before transaction acceptance
- Transaction uniqueness verification
- Complete transaction history tracking

### 4. Consensus Mechanism
- Proof of Work ensures agreement on valid chain
- Longest valid chain rule
- Computational cost deters malicious behavior

## Performance Considerations

### Mining Difficulty
- **Difficulty 1**: ~1,000 hash attempts
- **Difficulty 2**: ~16,000 hash attempts  
- **Difficulty 3**: ~256,000 hash attempts
- **Difficulty 4**: ~4M hash attempts (default)
- **Difficulty 5**: ~64M hash attempts
- **Difficulty 6**: ~1B hash attempts

### Memory Usage
- Each transaction: ~200 bytes
- Each block: ~1KB + transaction data
- Full blockchain scales linearly with transactions

### Scalability Limitations
- Single-threaded mining
- In-memory storage
- No network protocol
- Simplified signature scheme

## Real-World Blockchain Differences

This implementation demonstrates core concepts but differs from production blockchains:

### Simplified Areas:
1. **Cryptography**: Basic signature scheme vs. ECDSA
2. **Networking**: No P2P network protocol
3. **Consensus**: Single miner vs. distributed consensus
4. **Storage**: In-memory vs. persistent database
5. **Wallet**: Basic addresses vs. key management
6. **Scripts**: No smart contract support

### Production Features Not Included:
- Network protocol and peer discovery
- Advanced cryptographic signatures (ECDSA, Schnorr)
- UTXO model vs. account model
- Script/smart contract system
- Fee market and transaction prioritization
- Block size limits and optimization
- Fork resolution and reorganization
- Multi-signature transactions

## Educational Value

This implementation teaches:
1. **Hash Functions**: Understanding SHA-256 and its properties
2. **Merkle Trees**: Data integrity and efficient verification
3. **Proof of Work**: Consensus mechanism and mining
4. **Digital Signatures**: Transaction authorization
5. **Data Structures**: Linked blockchain structure
6. **Validation**: Comprehensive integrity checking
7. **Mining Economics**: Rewards and incentives

## Extending the Implementation

Possible enhancements:
1. **Network Layer**: Add P2P communication
2. **Advanced Crypto**: Implement ECDSA signatures
3. **UTXO Model**: Switch from account to UTXO
4. **Smart Contracts**: Add scripting capability
5. **Database**: Persistent storage with LevelDB
6. **Mining Pool**: Distributed mining support
7. **Wallet**: Key generation and management
8. **API**: REST/GraphQL interface

This foundation provides a solid understanding of blockchain fundamentals and can serve as a starting point for more advanced blockchain development.

## Running Tests

The test suite uses Catch2 v3 fetched via CMake FetchContent. To build and run:

```
cmake -B build -S . -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

**Test labels:** `ctest -L unit` runs unit tests only; `ctest -L integration` runs integration tests. P2P tests require serial execution (`-j1`).

**Known-failing tests** are tagged `[!mayfail]` and documented in `docs/superpowers/specs/2026-04-19-test-suite-design.md` §4. They expose real defects in the codebase (signature drop on deserialize, difficulty-model inconsistency, mineBlock target string bug) and are expected to stay red until those are fixed.

