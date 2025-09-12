# Blockchain Implementation in C++

A complete blockchain implementation from scratch in C++ featuring:

## Features

- **SHA-256 Hashing**: Secure cryptographic hashing for block integrity
- **Proof of Work**: Mining algorithm with adjustable difficulty
- **Transaction System**: Support for digital transactions with validation
- **Block Structure**: Complete block implementation with headers and merkle roots
- **Chain Validation**: Full blockchain validation and integrity checking
- **Mining Rewards**: Built-in mining reward system
- **Persistence**: Save and load blockchain to/from file

## Project Structure

```
blockchain/
├── include/           # Header files
│   ├── Block.h       # Block class definition
│   ├── Blockchain.h  # Blockchain class definition
│   ├── Transaction.h # Transaction class definition
│   └── utils.h       # Utility functions
├── src/              # Source files
│   ├── Block.cpp     # Block implementation
│   ├── Blockchain.cpp# Blockchain implementation
│   ├── Transaction.cpp# Transaction implementation
│   ├── utils.cpp     # Utility implementations
│   └── main.cpp      # Main application
└── CMakeLists.txt    # Build configuration
```

## Building

### Prerequisites
- C++17 compatible compiler
- CMake 3.16 or higher
- OpenSSL library

### Windows (Visual Studio)
```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Linux/macOS
```bash
mkdir build
cd build
cmake ..
make
```

## Usage

Run the blockchain application:
```
./blockchain
```

The application will:
1. Create a genesis block
2. Add sample transactions
3. Mine blocks
4. Validate the blockchain
5. Display the complete chain

## Core Concepts

### Block Structure
Each block contains:
- **Index**: Position in the chain
- **Timestamp**: When the block was created
- **Data**: Transaction data
- **Previous Hash**: Hash of the previous block
- **Merkle Root**: Root of the transaction merkle tree
- **Nonce**: Proof of work value
- **Hash**: SHA-256 hash of the block

### Mining
The mining process uses Proof of Work algorithm:
1. Calculate block hash
2. Check if hash meets difficulty target (starts with zeros)
3. If not, increment nonce and try again
4. When valid hash found, block is mined

### Transaction System
Transactions include:
- Sender and receiver addresses
- Amount being transferred
- Timestamp
- Digital signature verification

## License

MIT License - Feel free to use and modify as needed.
