# Full Transaction Signature Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `Transaction::isValid()` cryptographically verify the sender's signature by embedding the sender's public key in the transaction, and complete two dependent stubs (`verifySignatureByAddress`, P2P block handling).

**Architecture:** A `Transaction` gains a `sender_pubkey` field (compressed key, hex). Signing populates it. `isValid()` binds key→address (`deriveAddress(pubkey) == sender`) then verifies the ECDSA signature against the key. The signed payload and all hashes are unchanged, so merkle roots, block hashes, and mining are untouched. The public key is threaded through every serialization surface so restored/received signed transactions still validate. A new `Blockchain::addBlock()` validates and appends foreign blocks; `main.cpp` wires it into the P2P `onNewBlock` callback.

**Tech Stack:** C++23, w64devkit g++ 15.1, CMake (MinGW Makefiles), Catch2 v3. secp256k1 ECDSA via the in-repo `ECCrypto` / `BigInt`.

## Global Constraints

- C++23; compiler is w64devkit g++ at `C:/dev/w64devkit/bin/g++.exe` (hardcoded in CMakeLists).
- Test builds MUST be `-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON`. The naive BigInt makes ECC sign/verify minutes-slow at `-O0`; Release is required for tolerable runtime.
- Commit messages: NO Claude/Anthropic attribution of any kind (no `Co-Authored-By`, no trailer, no mention).
- Do NOT add the public key to `getTransactionData()` or `calculateHash()`. It is transported alongside, never signed or hashed. This keeps signatures/hashes/merkle roots byte-identical.
- Compressed public keys are 66 hex chars and contain no `,`, `|`, or whitespace — delimiter-safe in all formats.
- In the line-oriented file format (`Blockchain::saveToFile`/`loadFromFile`), empty fields use the `"-"` sentinel because `operator>>` skips blank tokens. The comma/pipe `getline` formats (`BlockSerializer`, `TransactionSerializer`) tolerate empty fields directly.

### Build & test commands (used throughout)

Configure once (from repo root):

```bash
cmake -S . -B build-tests -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
```

Build the test binary:

```bash
cmake --build build-tests --target blockchain_tests -j
```

Run all tests, or a subset by name regex:

```bash
ctest --test-dir build-tests --output-on-failure
ctest --test-dir build-tests --output-on-failure -R "isValid"
```

Build the main app (for tasks that touch `main.cpp`):

```bash
cmake -S . -B build-app -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-app --target blockchain -j
```

> Note: several TDD "verify it fails" steps fail at **compile time** (a test references a member that doesn't exist yet). In C++ that is the expected red — the whole `blockchain_tests` target fails to build with a clear "no member named ..." error until the implementation step adds it.

---

### Task 1: Transaction carries and populates the sender public key

Additive change: adds the field, accessors, and populates it on signing. `isValid()` is NOT changed yet, so every existing test stays green.

**Files:**
- Modify: `src/include/Transaction.h` (add field + getter/setter + private helper decl)
- Modify: `src/Transaction.cpp` (populate on signing)
- Test: `tests/unit/transaction_test.cpp`

**Interfaces:**
- Produces:
  - `std::string Transaction::getSenderPublicKey() const` — compressed pubkey hex, or `""` if unset.
  - `void Transaction::setSenderPublicKey(const std::string& pubkey_hex)`
  - `Transaction::signTransaction(const ECCrypto::PrivateKey&)` now also sets `sender_pubkey` on success.

- [ ] **Step 1: Write the failing test**

Add to `tests/unit/transaction_test.cpp`:

```cpp
TEST_CASE("signTransaction populates sender public key", "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction t(kf.address(), "receiver", 1.0);
    REQUIRE(t.getSenderPublicKey().empty());
    REQUIRE(t.signTransaction(kf.privHex()));
    REQUIRE(t.getSenderPublicKey() == kf.pubHex());
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-tests --target blockchain_tests -j`
Expected: compile FAIL — `blockchain_tests` does not build: "no member named 'getSenderPublicKey' in 'Transaction'".

- [ ] **Step 3: Add the field and accessors**

In `src/include/Transaction.h`, add the field after `signature` (currently line 13):

```cpp
    std::string signature;
    std::string sender_pubkey;
```

Add the accessors after `getSignature()` (currently line 26) and `setSignature()` (currently line 36). Place the getter with the other getters:

```cpp
    std::string getSignature() const { return signature; }
    std::string getSenderPublicKey() const { return sender_pubkey; }
```

and the setter next to `setSignature`:

```cpp
    void setSignature(const std::string& sig) { signature = sig; }
    void setSenderPublicKey(const std::string& pubkey_hex) { sender_pubkey = pubkey_hex; }
```

In the `private:` section, add a helper declaration next to `getTransactionData()`:

```cpp
private:
    std::string getTransactionData() const;
    std::string derivedAddressFromKey() const;
```

- [ ] **Step 4: Populate the key on signing**

In `src/Transaction.cpp`, in `signTransaction(const ECCrypto::PrivateKey& private_key)`, after `signature = ECCrypto::signatureToHex(sig);` (currently line 31) and before the success log, add:

```cpp
        signature = ECCrypto::signatureToHex(sig);
        // Attach the sender's public key so the transaction can be verified
        // downstream from the address alone. deriveAddress(pubkey) == sender is
        // enforced in isValid(). The key is NOT part of the signed data.
        BigInt priv = ECCrypto::bytes32ToBigInt(private_key.data());
        auto kp = ECCrypto::keyPairFromPrivateKey(priv);
        if (kp) {
            sender_pubkey = kp->public_key_hex;
        }
        std::println(stderr, "Transaction signed successfully");
        return true;
```

(`BigInt`, `bytes32ToBigInt`, and `keyPairFromPrivateKey` are all declared in `ECCrypto.h`, already included by `Transaction.cpp`.)

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build-tests --target blockchain_tests -j && ctest --test-dir build-tests --output-on-failure -R "populates sender public key"`
Expected: PASS.

- [ ] **Step 6: Run the full suite to confirm nothing regressed**

Run: `ctest --test-dir build-tests --output-on-failure`
Expected: all tests PASS (isValid is unchanged; the new field is purely additive).

- [ ] **Step 7: Commit**

```bash
git add src/include/Transaction.h src/Transaction.cpp tests/unit/transaction_test.cpp
git commit -m "feat(tx): carry and populate sender public key on signing"
```

---

### Task 2: Thread the public key through all serialization surfaces

Additive: makes the pubkey survive file save/load and both P2P serializers. Still no `isValid()` change, so the suite stays green; new tests assert the key roundtrips. This task MUST land before Task 3, because once `isValid()` requires the key, these paths would otherwise drop restored signed transactions.

**Files:**
- Modify: `src/Blockchain.cpp` (`saveToFile` ~lines 215-225, `loadFromFile` ~lines 261-273)
- Modify: `src/include/P2PMessage.h` (`BlockSerializer` ~lines 254-303, `TransactionSerializer` ~lines 313-337)
- Test: `tests/unit/p2p_message_test.cpp`, `tests/unit/blockchain_test.cpp`

**Interfaces:**
- Consumes: `Transaction::getSenderPublicKey()`, `Transaction::setSenderPublicKey()` (Task 1).
- Produces: all four serialization surfaces preserve `sender_pubkey` across a roundtrip.

- [ ] **Step 1: Write the failing tests**

Add to `tests/unit/p2p_message_test.cpp`:

```cpp
TEST_CASE("TransactionSerializer preserves sender public key", "[unit][p2p]") {
    test_support::KeyPairFixture kf;
    auto tx = kf.signedTx("bob", 3.5);
    auto restored = TransactionSerializer::deserialize(
        TransactionSerializer::serialize(*tx));
    REQUIRE(restored->getSenderPublicKey() == kf.pubHex());
    REQUIRE(restored->getSignature() == tx->getSignature());
}

TEST_CASE("BlockSerializer preserves signed transaction public key",
          "[unit][p2p]") {
    test_support::KeyPairFixture kf;
    Block b(1, "prevhash", 2);
    b.addTransaction(kf.signedTx("bob", 5.0));
    b.mineBlock();

    auto restored = BlockSerializer::deserialize(BlockSerializer::serialize(b));
    REQUIRE(restored->getTransactions().size() == 1);
    REQUIRE(restored->getTransactions()[0]->getSenderPublicKey() == kf.pubHex());
}
```

Add to `tests/unit/blockchain_test.cpp`:

```cpp
TEST_CASE("saveToFile/loadFromFile preserves signed transaction public key",
          "[unit][blockchain]") {
    MinedChainFixture f;
    KeyPairFixture alice;
    f.seedFunds(alice.address(), 100.0, "miner_1");
    auto tx = alice.signedTx("bob", 25.0);
    f.chain.addTransaction(tx);
    f.chain.minePendingTransactions("miner_1");

    TempDir tmp;
    std::string path = tmp.file("chain_pubkey.dat");
    REQUIRE(f.chain.saveToFile(path));

    Blockchain loaded(2, 50.0);
    REQUIRE(loaded.loadFromFile(path));
    auto loaded_tx = loaded.getLatestBlock()->getTransactions()[0];
    REQUIRE(loaded_tx->getSenderPublicKey() == tx->getSenderPublicKey());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build-tests --target blockchain_tests -j && ctest --test-dir build-tests --output-on-failure -R "preserves.*public key|preserves signed transaction public key"`
Expected: the three new tests FAIL — the restored transactions have empty `sender_pubkey` (`""` != the fixture pubkey), because serialization does not carry it yet.

- [ ] **Step 3: Thread pubkey through the file format**

In `src/Blockchain.cpp`, `saveToFile`, after the signature line (currently line 224):

```cpp
            file << (tx->getSignature().empty() ? "-" : tx->getSignature()) << std::endl;
            file << (tx->getSenderPublicKey().empty() ? "-" : tx->getSenderPublicKey()) << std::endl;
```

In `src/Blockchain.cpp`, `loadFromFile`, change the tx-reading block (currently lines 261-273) to:

```cpp
        for (size_t j = 0; j < tx_count; j++) {
            std::string sender, receiver, signature, pubkey;
            double amount;
            long long tx_timestamp;

            file >> sender >> receiver >> amount >> tx_timestamp >> signature >> pubkey;
            if (signature == "-") {
                signature.clear();
            }
            if (pubkey == "-") {
                pubkey.clear();
            }

            auto tx = std::make_shared<Transaction>(sender, receiver, amount,
                                                    tx_timestamp, signature);
            tx->setSenderPublicKey(pubkey);
            block->addTransaction(tx);
        }
```

- [ ] **Step 4: Thread pubkey through `BlockSerializer`**

In `src/include/P2PMessage.h`, `BlockSerializer::serialize`, extend the per-tx output (currently lines 255-259) to append the pubkey field:

```cpp
        for (const auto& tx : txs) {
            oss << "|" << tx->getSender()
                << "," << tx->getReceiver()
                << "," << std::format("{:.8f}", tx->getAmount())
                << "," << tx->getTimestamp()
                << "," << tx->getSignature()
                << "," << tx->getSenderPublicKey();
        }
```

In `BlockSerializer::deserialize`, change the inner tx parse (currently lines 291-303) to read and set the pubkey:

```cpp
            std::istringstream tx_stream(tx_data);
            std::string sender, receiver, sig, pubkey;
            double amount;
            long long tx_timestamp;

            std::getline(tx_stream, sender, ',');
            std::getline(tx_stream, receiver, ',');
            std::getline(tx_stream, token, ','); amount = std::stod(token);
            std::getline(tx_stream, token, ','); tx_timestamp = std::stoll(token);
            std::getline(tx_stream, sig, ',');
            std::getline(tx_stream, pubkey, ',');

            auto tx = std::make_shared<Transaction>(sender, receiver, amount,
                                                    tx_timestamp, sig);
            tx->setSenderPublicKey(pubkey);
            block->addTransaction(tx);
```

- [ ] **Step 5: Thread pubkey through `TransactionSerializer`**

In `src/include/P2PMessage.h`, `TransactionSerializer::serialize` (currently lines 313-321):

```cpp
    static std::string serialize(const Transaction& tx) {
        std::ostringstream oss;
        oss << tx.getSender() << "|"
            << tx.getReceiver() << "|"
            << std::format("{:.8f}", tx.getAmount()) << "|"
            << tx.getTimestamp() << "|"
            << tx.getSignature() << "|"
            << tx.getSenderPublicKey();
        return oss.str();
    }
```

`TransactionSerializer::deserialize` (currently lines 323-337):

```cpp
    static std::shared_ptr<Transaction> deserialize(const std::string& data) {
        std::istringstream iss(data);
        std::string sender, receiver, sig, pubkey, token;
        double amount;
        long long timestamp;

        std::getline(iss, sender, '|');
        std::getline(iss, receiver, '|');
        std::getline(iss, token, '|'); amount = std::stod(token);
        std::getline(iss, token, '|'); timestamp = std::stoll(token);
        std::getline(iss, sig, '|');
        std::getline(iss, pubkey, '|');

        auto tx = std::make_shared<Transaction>(sender, receiver, amount,
                                                timestamp, sig);
        tx->setSenderPublicKey(pubkey);
        return tx;
    }
```

- [ ] **Step 6: Run the new tests to verify they pass**

Run: `cmake --build build-tests --target blockchain_tests -j && ctest --test-dir build-tests --output-on-failure -R "preserves.*public key|preserves signed transaction public key"`
Expected: all three new tests PASS.

- [ ] **Step 7: Run the full suite**

Run: `ctest --test-dir build-tests --output-on-failure`
Expected: all tests PASS (existing save/load and p2p roundtrip tests still green; system transactions serialize with an empty/`-` pubkey field and parse back correctly).

- [ ] **Step 8: Commit**

```bash
git add src/Blockchain.cpp src/include/P2PMessage.h tests/unit/p2p_message_test.cpp tests/unit/blockchain_test.cpp
git commit -m "feat(serde): thread sender public key through file and P2P serialization"
```

---

### Task 3: Full `isValid()` and real `verifySignatureByAddress()`

Now that the key is populated and threaded, tighten validation to perform real cryptography.

**Files:**
- Modify: `src/Transaction.cpp` (`isValid` ~lines 108-140, `verifySignatureByAddress` ~lines 89-98, add `derivedAddressFromKey` helper)
- Test: `tests/unit/transaction_test.cpp`

**Interfaces:**
- Consumes: `getSenderPublicKey`, `verifySignature(const std::string&)`, `ECCrypto::deriveAddress`, `ECCrypto::hexToBytes`, `ECCrypto::PUBLIC_KEY_SIZE`, `ECCrypto::SIGNATURE_SIZE`.
- Produces: `isValid()` and `verifySignatureByAddress()` cryptographically verify signatures.

- [ ] **Step 1: Write the failing tests**

Add to `tests/unit/transaction_test.cpp`:

```cpp
TEST_CASE("isValid accepts a properly signed transaction", "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction t(kf.address(), "receiver", 1.0);
    REQUIRE(t.signTransaction(kf.privHex()));
    REQUIRE(t.isValid());
}

TEST_CASE("isValid rejects a public key that does not match the sender",
          "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction t(kf.address(), "receiver", 1.0);
    REQUIRE(t.signTransaction(kf.privHex()));
    // Swap in a different valid key: deriveAddress(pubkey) no longer == sender.
    auto other = ECCrypto::keyPairFromPrivateKey(BigInt(0x5678LL));
    t.setSenderPublicKey(other->public_key_hex);
    REQUIRE_FALSE(t.isValid());
}

TEST_CASE("isValid rejects a malformed signature", "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction t(kf.address(), "receiver", 1.0);
    REQUIRE(t.signTransaction(kf.privHex()));
    t.setSignature("deadbeef");  // valid hex, wrong length
    REQUIRE_FALSE(t.isValid());
}

TEST_CASE("isValid rejects a signature that does not match the transaction data",
          "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction signed_tx(kf.address(), "receiver", 1.0);
    REQUIRE(signed_tx.signTransaction(kf.privHex()));

    // Same sender/key (binding holds) but different data: ECDSA verify must fail.
    Transaction tampered(kf.address(), "receiver", 999.0);
    tampered.setSenderPublicKey(signed_tx.getSenderPublicKey());
    tampered.setSignature(signed_tx.getSignature());
    REQUIRE(tampered.getSenderPublicKey() == kf.pubHex());
    REQUIRE_FALSE(tampered.isValid());
}

TEST_CASE("verifySignatureByAddress accepts true address and rejects wrong",
          "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction t(kf.address(), "receiver", 1.0);
    REQUIRE(t.signTransaction(kf.privHex()));
    REQUIRE(t.verifySignatureByAddress(kf.address()));
    REQUIRE_FALSE(t.verifySignatureByAddress("not_the_address"));
}

TEST_CASE("verifySignatureByAddress requires a real signature, not just a matching address",
          "[unit][transaction]") {
    // Discriminating case: the old stub returned `sender == address`, so an
    // unsigned transaction whose address matches the sender would wrongly pass.
    // Real verification must reject it.
    test_support::KeyPairFixture kf;
    Transaction unsigned_tx(kf.address(), "receiver", 1.0);
    REQUIRE_FALSE(unsigned_tx.verifySignatureByAddress(kf.address()));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build-tests --target blockchain_tests -j && ctest --test-dir build-tests --output-on-failure -R "isValid|verifySignatureByAddress"`
Expected: four cases go RED against the current lenient code — "rejects a public key that does not match the sender", "rejects a malformed signature", "rejects a signature that does not match the transaction data", and "verifySignatureByAddress requires a real signature". The current `isValid()` returns `true` whenever a signature string is present, and the `verifySignatureByAddress` stub returns `sender == address` (so an *unsigned* tx with a matching address wrongly passes). The "accepts a properly signed transaction" and "accepts true address and rejects wrong" cases already pass — they are positive/regression assertions, not the red.

- [ ] **Step 3: Add the `derivedAddressFromKey` helper**

In `src/Transaction.cpp`, add near `getTransactionData()` (end of file):

```cpp
std::string Transaction::derivedAddressFromKey() const {
    if (sender_pubkey.length() != ECCrypto::PUBLIC_KEY_SIZE * 2) {
        return "";
    }
    ECCrypto::PublicKey pub_key;
    if (ECCrypto::hexToBytes(sender_pubkey, pub_key.data(),
                             ECCrypto::PUBLIC_KEY_SIZE) != ECCrypto::PUBLIC_KEY_SIZE) {
        return "";
    }
    return ECCrypto::deriveAddress(pub_key);
}
```

- [ ] **Step 4: Rewrite `isValid()`**

Replace the body from the signature check onward (currently lines 130-139) so the non-system path performs full verification:

```cpp
    // For mining reward transactions (from "system"), signature verification is skipped
    if (sender == "system") {
        return true;
    }

    // Must be signed with a well-formed signature.
    if (signature.empty()) {
        std::println(stderr, "Invalid transaction: Transaction not signed");
        return false;
    }
    if (signature.length() != ECCrypto::SIGNATURE_SIZE * 2) {
        std::println(stderr, "Invalid transaction: Malformed signature");
        return false;
    }

    // The attached public key must hash to the sender address (binds key->address).
    std::string derived = derivedAddressFromKey();
    if (derived.empty() || derived != sender) {
        std::println(stderr, "Invalid transaction: Public key does not match sender address");
        return false;
    }

    // Finally, the signature must verify against that public key.
    if (!verifySignature(sender_pubkey)) {
        std::println(stderr, "Invalid transaction: Signature verification failed");
        return false;
    }

    return true;
```

- [ ] **Step 5: Rewrite `verifySignatureByAddress()`**

Replace the stub body (currently lines 89-98) with:

```cpp
bool Transaction::verifySignatureByAddress(const std::string& address) const {
    if (signature.empty() || address.empty() || sender_pubkey.empty()) {
        return false;
    }
    // The attached key must belong to `address`, and the signature must verify.
    std::string derived = derivedAddressFromKey();
    if (derived.empty() || derived != address) {
        return false;
    }
    return verifySignature(sender_pubkey);
}
```

- [ ] **Step 6: Run the new tests to verify they pass**

Run: `cmake --build build-tests --target blockchain_tests -j && ctest --test-dir build-tests --output-on-failure -R "isValid|verifySignatureByAddress"`
Expected: all listed tests PASS.

- [ ] **Step 7: Run the full suite**

Run: `ctest --test-dir build-tests --output-on-failure`
Expected: all tests PASS. The save/load (`blockchain_test`, `chain_lifecycle_test`) and `p2p_loopback` tests are the regression guard — they push signed non-system transactions through reconstruction and then assert `isChainValid()`/balances, which now runs real verification on the threaded key.

- [ ] **Step 8: Commit**

```bash
git add src/Transaction.cpp tests/unit/transaction_test.cpp
git commit -m "feat(tx): verify signatures in isValid and verifySignatureByAddress"
```

---

### Task 4: `Blockchain::addBlock()` validates and appends a foreign block

**Files:**
- Modify: `src/include/Blockchain.h` (declare `addBlock`)
- Modify: `src/Blockchain.cpp` (implement `addBlock`; add `#include <unordered_set>`)
- Test: `tests/unit/blockchain_test.cpp`

**Interfaces:**
- Consumes: `Block::getIndex/getHash/getPreviousHash/getTimestamp/isValidWithDifficulty/getTransactions/calculateHash`, `Blockchain::calculateRequiredDifficultyAtHeight` (private), `updateBalances`.
- Produces: `bool Blockchain::addBlock(std::shared_ptr<Block> block)` — validates against the tip and appends; returns acceptance.

- [ ] **Step 1: Write the failing tests**

Add to `tests/unit/blockchain_test.cpp`:

```cpp
TEST_CASE("addBlock accepts a valid next block", "[unit][blockchain]") {
    MinedChainFixture f;
    auto tip = f.chain.getLatestBlock();
    int next_index = static_cast<int>(f.chain.getChainSize());
    int required = f.chain.calculateRequiredDifficulty();

    auto block = std::make_shared<Block>(next_index, tip->getHash(), required);
    block->addTransaction(std::make_shared<Transaction>("system", "miner", 50.0));
    block->mineBlock();

    REQUIRE(f.chain.addBlock(block));
    REQUIRE(f.chain.getChainSize() == 2);
    REQUIRE(f.chain.getLatestBlock()->getHash() == block->getHash());
}

TEST_CASE("addBlock rejects a block with the wrong previous hash",
          "[unit][blockchain]") {
    MinedChainFixture f;
    int next_index = static_cast<int>(f.chain.getChainSize());
    int required = f.chain.calculateRequiredDifficulty();

    auto block = std::make_shared<Block>(next_index, "wronghash", required);
    block->addTransaction(std::make_shared<Transaction>("system", "miner", 50.0));
    block->mineBlock();

    REQUIRE_FALSE(f.chain.addBlock(block));
    REQUIRE(f.chain.getChainSize() == 1);
}

TEST_CASE("addBlock rejects a block at the wrong index", "[unit][blockchain]") {
    MinedChainFixture f;
    auto tip = f.chain.getLatestBlock();
    int required = f.chain.calculateRequiredDifficulty();

    auto block = std::make_shared<Block>(5, tip->getHash(), required);
    block->addTransaction(std::make_shared<Transaction>("system", "miner", 50.0));
    block->mineBlock();

    REQUIRE_FALSE(f.chain.addBlock(block));
    REQUIRE(f.chain.getChainSize() == 1);
}

TEST_CASE("addBlock rejects a block with incorrect difficulty",
          "[unit][blockchain]") {
    MinedChainFixture f;
    auto tip = f.chain.getLatestBlock();
    int next_index = static_cast<int>(f.chain.getChainSize());
    int wrong = f.chain.calculateRequiredDifficulty() + 1;

    auto block = std::make_shared<Block>(next_index, tip->getHash(), wrong);
    block->addTransaction(std::make_shared<Transaction>("system", "miner", 50.0));
    block->mineBlock();

    REQUIRE_FALSE(f.chain.addBlock(block));
    REQUIRE(f.chain.getChainSize() == 1);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build-tests --target blockchain_tests -j`
Expected: compile FAIL — "no member named 'addBlock' in 'Blockchain'".

- [ ] **Step 3: Declare `addBlock`**

In `src/include/Blockchain.h`, in the `public:` section (after `bool isChainValid() const;`, line 34):

```cpp
    bool isChainValid() const;
    bool addBlock(std::shared_ptr<Block> block);
```

- [ ] **Step 4: Implement `addBlock`**

In `src/Blockchain.cpp`, add `#include <unordered_set>` to the includes (after `#include <algorithm>`, line 5). Then add the implementation (place it after `isChainValid()`):

```cpp
bool Blockchain::addBlock(std::shared_ptr<Block> block) {
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
```

- [ ] **Step 5: Run the new tests to verify they pass**

Run: `cmake --build build-tests --target blockchain_tests -j && ctest --test-dir build-tests --output-on-failure -R "addBlock"`
Expected: all four `addBlock` tests PASS.

- [ ] **Step 6: Run the full suite**

Run: `ctest --test-dir build-tests --output-on-failure`
Expected: all tests PASS.

- [ ] **Step 7: Commit**

```bash
git add src/include/Blockchain.h src/Blockchain.cpp tests/unit/blockchain_test.cpp
git commit -m "feat(chain): add addBlock to validate and append foreign blocks"
```

---

### Task 5: Wire P2P `onNewBlock` to `addBlock` and fix the demo sender

`main.cpp` is not unit-tested, so this task is verified by a clean build plus a manual smoke check.

**Files:**
- Modify: `src/main.cpp` (`onNewBlock` callback ~lines 47-51; case 1 transaction demo ~lines 231-237; add `#include "include/ECCrypto.h"`)

**Interfaces:**
- Consumes: `Blockchain::addBlock` (Task 4), `Transaction::signTransaction`, `ECCrypto::keyPairFromPrivateKeyHex`, `utils::sha256`.

- [ ] **Step 1: Include ECCrypto in main**

In `src/main.cpp`, add after the existing includes (after `#include "include/utils.h"`, line 4):

```cpp
#include "include/ECCrypto.h"
```

- [ ] **Step 2: Wire `onNewBlock` to `addBlock`**

Replace the `onNewBlock` callback body (currently lines 47-51):

```cpp
    callbacks.onNewBlock = [&blockchain](std::shared_ptr<Block> block) {
        std::cout << "\n[P2P] Received new block #" << block->getIndex() << std::endl;
        if (blockchain.addBlock(block)) {
            std::cout << "[P2P] Block #" << block->getIndex()
                      << " accepted" << std::endl;
        } else {
            std::cout << "[P2P] Block #" << block->getIndex()
                      << " rejected" << std::endl;
        }
    };
```

- [ ] **Step 3: Fix the case-1 demo so it produces a valid signed transaction**

Replace the case-1 body that builds and signs the transaction (currently lines 231-237):

```cpp
                // Demo key management: derive a deterministic private key from
                // the sender name, then transact from the address that key
                // actually controls. isValid() now binds the signature to
                // deriveAddress(pubkey), so a free-text sender is rejected.
                std::string demo_priv = utils::sha256(sender + "_private_key");
                auto demo_kp = ECCrypto::keyPairFromPrivateKeyHex(demo_priv);
                if (!demo_kp) {
                    std::cout << "Failed to derive a key for sender '" << sender
                              << "'" << std::endl;
                    break;
                }
                std::cout << "Using address " << demo_kp->address
                          << " for '" << sender << "'" << std::endl;
                auto transaction = std::make_shared<Transaction>(
                    demo_kp->address, receiver, amount);
                transaction->signTransaction(demo_priv);
                blockchain.addTransaction(transaction);
```

- [ ] **Step 4: Build the app to verify it compiles**

Run:
```bash
cmake -S . -B build-app -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-app --target blockchain -j
```
Expected: `blockchain.exe` builds with no errors.

- [ ] **Step 5: Manual smoke check**

Run `build-app/blockchain.exe`, then at the menu:
1. Choose `1` (Add Transaction); enter sender `alice`, receiver `bob`, amount `10`. Expect: prints `Using address <hex> for 'alice'`, then `Transaction added to pending pool` (NOT "Invalid transaction rejected!").
2. Choose `2` (Mine Block); enter miner `m`. Expect a block mines and is added.
3. Choose `5` (Validate Blockchain). Expect `Blockchain is VALID`.
4. Choose `0` to exit.

Expected: the signed demo transaction is accepted and the chain validates — confirming end-to-end signing + verification.

- [ ] **Step 6: Confirm the test suite is still green**

Run: `ctest --test-dir build-tests --output-on-failure`
Expected: all tests PASS.

- [ ] **Step 7: Commit**

```bash
git add src/main.cpp
git commit -m "feat(p2p): validate received blocks via addBlock; fix demo tx signing"
```

---

## Definition of done

- `Transaction::isValid()` verifies a real ECDSA signature (well-formed signature, `deriveAddress(pubkey) == sender`, signature verifies) for non-system transactions; `system` reward transactions remain exempt.
- `Transaction::verifySignatureByAddress()` performs real verification.
- The sender public key is threaded through the file format and both P2P serializers; the full suite passes.
- `Blockchain::addBlock()` validates and appends foreign blocks; `main.cpp`'s `onNewBlock` uses it.
- The interactive demo produces a valid signed transaction that validates.
- Every commit builds and leaves `ctest --test-dir build-tests --output-on-failure` fully green.
