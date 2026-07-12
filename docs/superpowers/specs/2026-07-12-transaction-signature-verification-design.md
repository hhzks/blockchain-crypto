# Full Transaction Signature Verification via Embedded Public Keys

**Date:** 2026-07-12
**Status:** Approved (design)

## Problem

`Transaction::isValid()` (src/Transaction.cpp) is a stub for its most important
job. It performs structural checks (non-empty parties, positive amount, no
self-send, a `system` mining-reward bypass) and then, for a normal transaction,
only checks that the signature string is non-empty:

```cpp
// In a real implementation, we would verify the signature here
// For now, we'll just check that a signature exists
// Full verification requires the public key of the sender
return true;
```

The blocker is structural, not a missing line. A `Transaction` carries only an
*address* (`sender`), and addresses are derived one-way —
`deriveAddress(pubkey)` = first 20 bytes of `SHA-256(pubkey)`
(src/ECCrypto.cpp:207). A public key cannot be recovered from an address, and
ECDSA verification requires the public key. So the transaction has no way to
verify its own signature.

Two sibling spots share the same root cause and are in scope:

- `Transaction::verifySignatureByAddress()` (src/Transaction.cpp:89) is a stub
  that returns `sender == address` and verifies nothing cryptographically.
- The P2P `onNewBlock` callback (src/main.cpp:49) receives a block but never
  validates or adds it, and there is no `Blockchain::addBlock()` for it to call —
  received blocks currently have nowhere to go.

## Chosen approach

**Embed the sender's compressed public key in the transaction.** `isValid()`
then enforces the real security property in two steps:

1. **Key binds to address:** `deriveAddress(sender_pubkey) == sender`.
2. **Signature binds to key:** the ECDSA signature verifies against
   `sender_pubkey`.

Together these prove the holder of the private key behind `sender` authorized
this exact transaction. Dropping step 1 would let an attacker attach any key
they control and "spend" from an address they do not own, so the binding check
is essential.

### Key property that bounds the blast radius

The signed payload (`getTransactionData()` = `sender:receiver:amount:timestamp`)
and `calculateHash()` are **unchanged** — the public key is transported
alongside the transaction, not folded into the signed data or the hash.
Therefore existing signatures, transaction hashes, merkle roots, and block
hashes are all bit-identical. Nothing about mining or the chain's hash structure
changes. Only two things change semantically: transactions now carry an extra
field, and validation now performs real cryptography.

### Alternatives considered

- **ECDSA public-key recovery** (recover the key from the signature, then check
  the derived address). No data-model change, but requires substantial new
  recoverable-ECDSA crypto — the current 64-byte signature has no recovery id —
  and carries the highest correctness risk. Rejected.
- **Structural-only hardening** (require a well-formed signature but no crypto).
  Minimal and contained, but does not actually verify the signature, so it does
  not satisfy "full implementation." Rejected.

## Changes

### 1. Transaction data model (src/include/Transaction.h, src/Transaction.cpp)

- Add private field `std::string sender_pubkey;` — the compressed public key as a
  66-hex-char string, mirroring how `signature` is already stored as hex.
- Add `getSenderPublicKey()` / `setSenderPublicKey()`, mirroring the existing
  `getSignature()` / `setSignature()` pair. The setter is how deserialization
  re-attaches the key, so **no constructor signature changes** — the existing
  restore constructor and its test stay untouched.

### 2. Signing populates the key (`Transaction::signTransaction`)

In the `PrivateKey` overload (the hex overload already delegates to it), after a
successful sign, derive the compressed public key from the private key
(`bytes32ToBigInt` -> `keyPairFromPrivateKey`) and store its `public_key_hex` in
`sender_pubkey`. Every existing signing path — `Wallet::signTransaction`, the
test fixtures' `signedTx()` — auto-populates the key with no caller changes.

### 3. `isValid()` full implementation

Keep the existing structural checks and the `system` bypass. For a non-system
transaction, additionally:

- reject if `sender_pubkey` is empty;
- reject if `signature` is malformed (length != `SIGNATURE_SIZE * 2` hex chars) —
  closes the well-formedness gap;
- reject if `deriveAddress(sender_pubkey) != sender`;
- return `verifySignature(sender_pubkey)` (reuses the existing, tested ECDSA
  path).

### 4. `verifySignatureByAddress()` real implementation

Replace the `sender == address` stub with: return false if signature, pubkey, or
address is empty; then require `deriveAddress(sender_pubkey) == address` **and**
`verifySignature(sender_pubkey)`. It now proves the signature came from the
holder of `address`.

### 5. Serialization — thread the pubkey through all four surfaces

Each surface reconstructs a transaction and (directly or via
`Block::addTransaction`) runs `isValid()`, so each must carry the key or restored
signed transactions are silently dropped:

- **`Blockchain::saveToFile` / `loadFromFile`** — add one pubkey line per
  transaction, using the existing `"-"` sentinel for empty/system keys (the same
  trick already used for empty signatures, because `operator>>` skips blank
  tokens).
- **`BlockSerializer`** (src/include/P2PMessage.h) — append the pubkey as another
  comma-separated field; on deserialize, call `setSenderPublicKey()` after
  construction.
- **`TransactionSerializer`** (src/include/P2PMessage.h) — append the pubkey as
  another pipe-separated field; same on deserialize.

Compressed-pubkey hex contains no `,`, `|`, or whitespace, so it is
delimiter-safe in every format.

The on-disk and P2P wire formats change; pre-existing `.dat` files will not
reload. This is acceptable — the only such files are ephemeral test
temporaries.

### 6. `Blockchain::addBlock()` + P2P wiring

- New `bool Blockchain::addBlock(std::shared_ptr<Block> block)` validates a
  foreign block before appending: `index == chain.size()`,
  `previousHash == tip hash`, `timestamp >= tip timestamp`, and
  `isValidWithDifficulty(calculateRequiredDifficultyAtHeight(index))` — which
  already re-runs full transaction validation, now including real signatures. On
  success: append, drop any now-included pending transactions, call
  `updateBalances()`. Returns whether the block was accepted.
- **`main.cpp` `onNewBlock`** (line 49): replace the dead comment with a call to
  `blockchain.addBlock(block)` and report accepted/rejected.

### 7. Interactive demo (src/main.cpp case 1)

The demo signs a transaction from a free-text sender (e.g. `"alice"`). The
stricter `isValid()` now rejects it because `deriveAddress(pubkey) != "alice"`.
**Decision (approved):** derive the keypair from the typed name, set the
transaction's sender to the *derived address*, and print it (e.g. `"Using
address <addr> for 'alice'"`) so the demo still produces valid transactions.

## Testing (TDD; the repo runs the Release test suite)

New cases, written before implementation:

- **transaction_test:** a signed tx from a real address is `isValid()`;
  swapping in a valid-but-wrong-address pubkey is rejected; a malformed
  signature is rejected; `verifySignatureByAddress` accepts the true address and
  rejects a wrong one; `sender_pubkey` is populated by signing.
- **Serialization:** `TransactionSerializer` and `BlockSerializer` roundtrips
  preserve `sender_pubkey`, and the restored signed transaction is `isValid()`.
- **blockchain_test (`addBlock`):** accepts a valid next block; rejects
  wrong-previous-hash, wrong-index, and bad-difficulty blocks.
- **Existing tests stay green unchanged.** The save/load roundtrip
  (blockchain_test, chain_lifecycle_test) and P2P loopback tests are the
  regression guard proving the pubkey threads correctly through every
  serialization surface; both push signed non-system transactions through
  reconstruction and then assert `isChainValid()` / correct balances.

## Risks and considerations

- **Real crypto now runs in validation paths** (`isValid` -> `Block::isValid` ->
  `isChainValid` / `addBlock`), each performing an ECDSA verify (two scalar
  multiplications on the naive BigInt). The suite contains only a handful of
  signed transactions, so the impact is small, but test runtime will be watched.
- No change to hashing, merkle roots, or proof-of-work, so there is no
  chain-structure regression surface.
