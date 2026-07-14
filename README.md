# Blockchain from Scratch (C++23)

A small, self-contained blockchain implemented from first principles in modern
C++. It exists to make the moving parts of a blockchain legible: proof-of-work mining, a hand-written SHA-256, a
hand-written secp256k1 ECDSA (on a custom big-integer type), Merkle roots,
wallet key management, on-disk persistence, and a peer-to-peer node.

## Features

- **Transactions** with real ECDSA signatures over secp256k1. `isValid()`
  verifies that the attached public key hashes to the sender address
  (`deriveAddress(pubkey) == sender`) **and** that the signature verifies (so a transaction proves the sender authorized it).
- **Blocks** with Merkle roots, previous-hash linkage, and proof-of-work.
- **Mining** with automatic difficulty adjustment (retargets every 10 blocks
  toward a target block time) and a mining reward.
- **Chain validation** end to end (`isChainValid`), plus `addBlock()` for
  validating and appending a block received from a peer.
- **Wallets**: keypair generation/import, address derivation, transaction
  signing, and a keystore file (keys are currently saved as plaintext hex — the
  `password` argument is accepted but not yet applied).
- **Persistence**: save/load the whole chain to a text file.
- **P2P networking**: a TCP node (Winsock on Windows, POSIX sockets elsewhere)
  that handshakes with peers, gossips transactions and blocks, and syncs.
- **From-scratch primitives**: SHA-256 (`sha.cpp`), a `BigInt` (`bigint.h`),
  and secp256k1 point arithmetic + ECDSA (`ECCrypto.cpp`).
- **Interactive CLI** for driving all of the above.

## Project structure

```text
src/
  main.cpp            Interactive menu-driven CLI
  Transaction.{h,cpp} Transaction: hashing, signing, signature verification
  Block.{h,cpp}       Block: Merkle root, proof-of-work, validation
  Blockchain.{h,cpp}  Chain: mining, balances, save/load, addBlock, validation
  Wallet.{h,cpp}      Keypair management, signing, keystore
  ECCrypto.{h,cpp}    secp256k1 ECDSA, address derivation, key (de)serialization
  bigint.h            Arbitrary-precision integer used by the crypto
  sha.{h,cpp}         SHA-256
  utils.{h,cpp}       Hashing helpers, timestamps, proof-of-work check, Merkle
  P2PNode.{h,cpp}     TCP peer-to-peer node
  P2PMessage.h        Wire protocol + block/transaction serializers
tests/                Catch2 v3 unit + integration tests
```

## Requirements

- A **C++23** compiler. Development is done with **w64devkit g++ 15.1** on
  Windows. `std::print`/`std::format` require GCC 14+ (and linking
  `libstdc++exp`, which the build already handles).
- **CMake ≥ 3.25**.
- On Windows, P2P uses Winsock (`ws2_32`, linked automatically); other
  platforms use POSIX sockets.

> The compiler path is currently pinned in `CMakeLists.txt`
> (`C:/dev/w64devkit/bin/g++.exe`). If your toolchain lives elsewhere, edit
> those `CMAKE_CXX_COMPILER`/`CMAKE_C_COMPILER` lines (or remove them to let
> CMake auto-detect).

## Build & run

```bash
# Configure and build the app (Release is strongly recommended — see note)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target blockchain

# Run the interactive CLI
./build/blockchain          # ./build/blockchain.exe on Windows
```

> **Build Release, not Debug.** The `BigInt` arithmetic is intentionally naive;
> at `-O0` the ECDSA sign/verify path is minutes-slow. `-DCMAKE_BUILD_TYPE=Release`
> makes it fast. On Windows you may also want an explicit generator, e.g.
> `-G "MinGW Makefiles"` or `-G Ninja`.

### The CLI

The menu drives the chain and the P2P node:

```text
1.  Add Transaction        7.  Load Blockchain
2.  Mine Block             8.  Start P2P Node
3.  Check Balance          9.  Stop P2P Node
4.  Display Blockchain     10. Connect to Peer
5.  Validate Blockchain    11. Show Connected Peers
6.  Save Blockchain        12. Request Blockchain Sync
0.  Exit                   13. P2P Node Status
```

For the demo, "Add Transaction" derives a deterministic keypair from the sender
name and transacts from the address that key actually controls (so the signed
transaction passes verification). Note that a freshly started chain has no
funds, so a transaction is admitted to the mempool only once its sender has a
balance.

## Tests

The suite uses [Catch2 v3](https://github.com/catchorg/Catch2), fetched
automatically via CMake `FetchContent` (needs network access on the first
configure).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --target blockchain_tests
ctest --test-dir build --output-on-failure
```

## How it works

- **Addresses.** A private key is a 256-bit scalar; the public key is the
  compressed secp256k1 point (33 bytes). An address is the first 20 bytes of
  `SHA-256(public key)`, rendered as hex (Ethereum-style).
- **Signing.** `signTransaction` signs `sender:receiver:amount:timestamp` with
  ECDSA and stores the signature plus the sender's public key on the
  transaction. The public key is transported alongside the transaction — it is
  never folded into the transaction hash or the signed payload, so hashes and
  Merkle roots stay stable.
- **Validation.** `Transaction::isValid()` (for non-`system` transactions)
  requires a well-formed signature, checks `deriveAddress(pubkey) == sender`,
  then verifies the signature against that key. `system` transactions (mining
  rewards) are exempt. `addBlock()` additionally enforces that a block carries
  exactly one mining-reward transaction of the expected amount.
- **Mining.** `mineBlock` searches for a nonce whose block hash satisfies the
  difficulty target; difficulty retargets on a fixed interval.

## Security model & limitations

This is a learning project. Known, intentional gaps:

- The SHA-256, `BigInt`, and ECDSA code is hand-rolled, **unaudited, and not
  constant-time**.
- Balances are tracked by address string and recomputed from chain history.
- `addBlock()` validates structure, proof-of-work, difficulty, per-transaction
  signatures, and the mining-reward invariant, but does **not** check that
  non-reward senders in a received block have sufficient balance (no
  double-spend/overspend check for foreign blocks). `isChainValid()` shares this
  gap.
- The persisted chain-file format includes a per-transaction public key; files
  written by older builds will not load.
