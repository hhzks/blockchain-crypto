# Blockchain Test Suite — Design Spec

**Date:** 2026-04-19
**Scope:** Rigorous automated test suite for the C++23 blockchain project covering core modules (utils, sha, bigint, ECCrypto, Transaction, Block, Blockchain, Wallet) plus P2P message serialization and loopback socket integration. Excludes full P2P sync/peer-discovery threading paths, fuzzing, and interactive `main.cpp`.

## Goals

- Catch regressions in cryptographic correctness, consensus rules, serialization, and chain validation.
- Deterministic, fast (<30s full suite), no external network dependencies.
- Low-friction local and CI runs via CMake + CTest.
- Surface existing defects without codifying them.

## Non-Goals

- Fuzzing (libFuzzer), property-based testing (rapidcheck), mutation testing.
- P2P peer-discovery, sync threads, timeout reconnection.
- Interactive menu in `src/main.cpp`.
- Cross-implementation crypto validation (e.g., against OpenSSL/libsecp256k1).

## Decisions (from brainstorming)

| Axis | Decision |
|---|---|
| Scope | Core crypto + blockchain + P2P message serialization + loopback P2P integration |
| Framework | Catch2 v3 via CMake `FetchContent` |
| Layout | `tests/` tree mirroring `src/`, discovered with `catch_discover_tests` |
| Build integration | `-DBUILD_TESTS=ON` opt-in flag |
| Crypto rigor | Known-answer vectors (NIST SHA-256, Bitcoin secp256k1) + roundtrip |
| Testability | Targeted minimal refactors permitted to make behavior observable |
| Bug handling | Write tests asserting *correct* behavior; tag bug-gated tests `[!mayfail]` |

## Architecture

### Directory layout

```
blockchain/
├── CMakeLists.txt              (modified)
├── src/                        (production code; minimal refactors)
└── tests/
    ├── CMakeLists.txt
    ├── support/
    │   ├── fixtures.h          (MinedChainFixture, KeyPairFixture, LoopbackPeerFixture)
    │   └── vectors.h           (NIST SHA-256 + Bitcoin secp256k1 known answers)
    ├── unit/
    │   ├── utils_test.cpp
    │   ├── sha_test.cpp
    │   ├── bigint_test.cpp
    │   ├── eccrypto_test.cpp
    │   ├── transaction_test.cpp
    │   ├── block_test.cpp
    │   ├── blockchain_test.cpp
    │   ├── wallet_test.cpp
    │   └── p2p_message_test.cpp
    └── integration/
        ├── chain_lifecycle_test.cpp
        └── p2p_loopback_test.cpp
```

### CMake integration

1. Split `add_executable(blockchain ${SOURCES} ${HEADERS})` into:
   - `add_library(blockchain_core STATIC ${SOURCES} ${HEADERS})` (excludes `src/main.cpp`)
   - `add_executable(blockchain src/main.cpp)` linking `blockchain_core`
2. Add `option(BUILD_TESTS "Build the test suite" OFF)`.
3. When `ON`: `FetchContent` Catch2 v3, include `tests/CMakeLists.txt` which defines `blockchain_tests` executable linking `blockchain_core` + `Catch2::Catch2WithMain`, and calls `catch_discover_tests(blockchain_tests)`.
4. Labels: unit tests tagged with `[unit]`, integration with `[integration]`, P2P with `[p2p]`. `ctest -L unit` runs parallelisable subset; full suite runs `-j1`.

## Components Under Test

### utils (unit)
- `sha256`: output is 64-char lowercase hex; empty-string matches known digest; determinism.
- `calculateMerkleRoot`: 0-tx, 1-tx, 2-tx, 3-tx (odd-count duplication), 4-tx cases match expected root.
- `checkProofOfWork`: boundary at each difficulty 0..5.
- `bytesToHex`: roundtrip with `hexToBytes` equivalent.

### sha (unit)
- NIST FIPS-180-4 vectors: `""`, `"abc"`, `"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"`, 1,000,000 × `'a'`.
- Output matches `utils::sha256` for same input (cross-consistency).

### bigint (unit)
- Construction from decimal string, hex string, `int64_t`.
- Arithmetic operators `+ - * / %` against `__int128` reference for values ≤ 2^63.
- Modular exponentiation matches Python reference for curve parameter operations.
- `modInverse` correctness on secp256k1 field.
- `bigIntToBytes32` / `bytes32ToBigInt` roundtrip.
- Comparison operators on edge cases (0, negative, equality).

### ECCrypto (unit)
- `isValidPrivateKey`: rejects 0, rejects ≥ N, accepts N-1 and 1.
- Priv `0x1` → pub = G (known generator coordinates).
- Bitcoin test vector: known privkey → expected compressed pubkey → expected P2PKH address.
- `signHash` → `verifySignature` roundtrip with fresh keypair.
- Tampered signature fails verification.
- `compressPublicKey` / `decompressPublicKey` roundtrip.
- `testECCImplementation()` returns true.

### Transaction (unit)
- Constructor populates sender/receiver/amount/timestamp.
- `calculateHash`: deterministic for identical inputs; changes on any field change.
- `signTransaction` + `verifySignature` roundtrip (both hex and struct overloads).
- `verifySignature` with wrong public key returns false.
- `isValid`: rejects empty sender, empty receiver, negative amount, self-send (sender == receiver).

### Block (unit)
- `calculateMerkleRoot` matches `utils::calculateMerkleRoot` of tx hashes.
- `mineBlock` at difficulty 2 produces hash with ≥2 leading `0` nibbles.
- `mineBlock` increments nonce from 0.
- `isValidWithDifficulty`: catches tampered transaction (re-signs required), tampered stored hash, wrong merkle root.
- `isMined(difficulty)` agrees with `checkProofOfWork` on the block's hash.

### Blockchain (unit + integration)
- Genesis block is index 0, previous hash `"0"`, mined at `INITIAL_DIFFICULTY`.
- `addTransaction`: accepts valid system tx, rejects duplicate, rejects insufficient balance (non-system sender), rejects `isValid()==false` tx.
- `minePendingTransactions`: clears pending pool, appends block to chain, credits miner with `mining_reward`.
- `getBalance` after 3-block chain with multiple transfers.
- `isChainValid`:
  - True for freshly mined chain.
  - False after mutating a transaction's amount (tamper detection).
  - False after swapping `previousHash` of block N (link break).
  - False after setting block N+1 timestamp ≤ block N timestamp.
- `saveToFile` / `loadFromFile` roundtrip: chain equality (hashes, indexes, tx count, balances).

### Wallet (unit)
- `generateNewAddress` produces unique addresses across 10 calls.
- Generated address matches `wallet::utils::isValidAddress`.
- `importPrivateKey(hex)` → `getPublicKeyHex(address)` derives expected public key.
- `signTransaction(tx, address)` + `verifyTransaction(tx, address)` roundtrip.
- `saveToFile(path, pwd)` / `loadFromFile(path, pwd)` roundtrip preserves keys.
- Loading with wrong password fails.

### P2PMessage (unit)
- For each `MessageType`: serialize → deserialize → equal.
- `deserialize` rejects: < `HEADER_SIZE` bytes, wrong magic, payload length > `MAX_PAYLOAD_SIZE`, truncated payload (length field says N, buffer has < N).
- `PeerInfo::serialize` / `deserialize` roundtrip.
- Factory helpers (`createHandshake`, `createPing`, etc.) produce messages whose payloads reparse to expected fields.

### P2PNode loopback (integration)
- Two `P2PNode`s on 127.0.0.1, OS-assigned ports (port 0), connect and handshake within 2s.
- After handshake, `broadcastTransaction` on node A results in `onNewTransaction` callback firing on node B within 2s with matching sender/receiver/amount.
- Ping/pong exchange observable via callback or state.
- Graceful `stop()` on both nodes joins all threads (verified by destructor not timing out).

### Chain lifecycle (integration)
End-to-end: wallet → signed tx → mine → validate → save → load → validate. See design section 3 for full flow.

## Fixtures

### `KeyPairFixture`
Fixed private key `0x1234...` yields deterministic public key and address. Helper `signedTx(from, to, amount, kp)` builds and signs in one call.

### `MinedChainFixture`
Constructs `Blockchain`, pre-mines 3 blocks with fixture wallets at `INITIAL_DIFFICULTY=2`. Per-test cost ~few ms.

### `LoopbackPeerFixture`
Spins up two `P2PNode` on ephemeral ports, waits on `condition_variable` for handshake, exposes `waitForMessage(type, timeout_ms)` with 2s default. Destructor calls `stop()` and joins threads.

## Data Flow (sample integration test)

```
1. Wallet → generate alice, bob
2. Blockchain bootstrap: system→alice tx, mine
3. Assert: alice balance > 0, bob balance == 0
4. Signed tx alice→bob, addTransaction, mine
5. Assert: balances reflect transfer + mining rewards
6. Assert: isChainValid() == true
7. saveToFile(tmp_path) → new Blockchain → loadFromFile(tmp_path)
8. Assert: loaded == original (block hashes, tx count, chain length)
```

Timestamps: tests never equality-compare timestamps across runs; use range bounds or compare pre-save vs post-load directly.

## Known Bugs Surfaced

Writing tests against current code uncovered these defects. The suite asserts **correct** behavior; failing tests are tagged `[!mayfail]` with a comment pointing at this spec section.

| # | Bug | Test that fails |
|---|---|---|
| 1 | `BlockSerializer::deserialize`, `TransactionSerializer::deserialize`, and `Blockchain::loadFromFile` parse signatures but drop them — constructor doesn't restore them. | Transaction/Block save-load-verify roundtrip |
| 2 | `Blockchain()` sets `difficulty=4`/`mining_reward=100` but `INITIAL_DIFFICULTY=2`/`INITIAL_MINING_REWARD=50`. Inconsistent with genesis and validation. | "genesis difficulty matches blockchain difficulty member" |
| 3 | `calculateRequiredDifficulty` (time-based) and `calculateRequiredDifficultyAtHeight` (simulated progression) disagree. `isChainValid` uses the latter, so chains mined with the former eventually fail self-validation. | "mine 20 blocks, then isChainValid() == true" |
| 4 | `saveToFile` hardcodes `"blockchain_saves/"` prefix; fails silently if directory missing. | Roundtrip test in `std::filesystem::temp_directory_path()` |

These fixes are out of scope for this task. The implementation plan may propose them as follow-ups; the test suite will document and mark them.

## Required Minimal Refactors (for testability)

1. **Split `blockchain` target** into `blockchain_core` library + `blockchain` executable.
2. **Expose deterministic Blockchain construction**: constructor overload `Blockchain(int initial_difficulty, double reward)` so tests don't depend on internal defaults.
3. **Allow signature restoration**: add `Transaction::setSignature(const std::string&)` (or a deserialization constructor). Without this, bug #1 can't be fixed; tests still pass pre-fix by being `[!mayfail]`.
4. **Expose P2P actual listen port**: `P2PNode::getActualListenPort()` for tests using port 0.
5. **`saveToFile` accepts full path** (no hardcoded prefix), OR tests create the `blockchain_saves/` dir explicitly. Plan picks one.

## Error Handling

- `REQUIRE` / `CHECK` with `INFO` context.
- `REQUIRE_THROWS_AS` for documented exception paths (malformed messages).
- Bounded `wait_for` with 2s default on all P2P test waits; timeout fails with state dump.
- Bug-gated tests tagged `[!mayfail]`, commented with spec §4 reference.

## Platform & CI

- Windows (MinGW/w64devkit): Winsock init via existing P2PNode code; tests don't touch `WSAStartup` directly.
- Linux: unchanged code path.
- Filesystem tests use `std::filesystem::temp_directory_path() / "blockchain_test_XXXX"`; fixture destructor cleans up.
- Port collisions avoided by binding port 0, reading back via `getsockname`.
- Runtime budget: < 30s full suite. Crypto/bigint ≤ 5s, blockchain mining ≤ 15s, P2P loopback ≤ 10s.
- Run: `cmake -B build -DBUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure`.
- Parallelism: `ctest -L unit -jN` safe; full `ctest -j1` because P2P sockets.

## Success Criteria

- `cmake --build build` produces `blockchain_tests` when `BUILD_TESTS=ON`.
- `ctest --output-on-failure` exits 0 except for the `[!mayfail]`-tagged tests tied to known bugs.
- All correctness-critical paths listed in the coverage matrix have at least one test case.
- Full suite completes in under 30 seconds on a developer workstation.
- No test depends on external network, wall-clock equality, or interactive input.

## Future Work (out of scope here)

- libFuzzer harnesses for `Message::deserialize`, `BlockSerializer::deserialize`, `ECCrypto::signatureFromHex`.
- Rapidcheck property tests for bigint arithmetic laws.
- Full P2P sync test (multi-node convergence).
- Coverage reporting target (`gcovr`) as CMake custom target.
