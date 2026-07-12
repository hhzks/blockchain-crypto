#include <catch2/catch_test_macros.hpp>
#include "Transaction.h"
#include "ECCrypto.h"
#include "bigint.h"
#include "fixtures.h"

TEST_CASE("Transaction constructor populates fields", "[unit][transaction]") {
    Transaction t("alice", "bob", 10.0);
    REQUIRE(t.getSender() == "alice");
    REQUIRE(t.getReceiver() == "bob");
    REQUIRE(t.getAmount() == 10.0);
    REQUIRE(t.getSignature().empty());
}

TEST_CASE("calculateHash is deterministic", "[unit][transaction]") {
    Transaction t("alice", "bob", 5.0);
    REQUIRE(t.calculateHash() == t.calculateHash());
}

TEST_CASE("calculateHash changes on any field change", "[unit][transaction]") {
    Transaction a("alice", "bob", 5.0);
    Transaction b("alice", "carol", 5.0);
    Transaction c("alice", "bob", 6.0);
    REQUIRE(a.calculateHash() != b.calculateHash());
    REQUIRE(a.calculateHash() != c.calculateHash());
}

TEST_CASE("sign with priv then verify with pub roundtrips", "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction t(kf.address(), "receiver", 1.0);
    REQUIRE(t.signTransaction(kf.privHex()));
    REQUIRE(t.verifySignature(kf.pubHex()));
}

TEST_CASE("verify with wrong key fails", "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction t(kf.address(), "receiver", 1.0);
    REQUIRE(t.signTransaction(kf.privHex()));

    auto other = ECCrypto::keyPairFromPrivateKey(BigInt(0x5678LL));
    REQUIRE_FALSE(t.verifySignature(other->public_key_hex));
}

TEST_CASE("isValid rejects empty sender/receiver", "[unit][transaction]") {
    Transaction t1("", "bob", 1.0);
    Transaction t2("alice", "", 1.0);
    REQUIRE_FALSE(t1.isValid());
    REQUIRE_FALSE(t2.isValid());
}

TEST_CASE("isValid rejects non-positive amount", "[unit][transaction]") {
    Transaction t1("alice", "bob", 0.0);
    Transaction t2("alice", "bob", -5.0);
    REQUIRE_FALSE(t1.isValid());
    REQUIRE_FALSE(t2.isValid());
}

TEST_CASE("isValid rejects self-send", "[unit][transaction]") {
    Transaction t("alice", "alice", 1.0);
    REQUIRE_FALSE(t.isValid());
}

TEST_CASE("isValid accepts system mining reward without signature", "[unit][transaction]") {
    Transaction t("system", "miner", 50.0);
    REQUIRE(t.isValid());
}

TEST_CASE("isValid rejects unsigned non-system transaction", "[unit][transaction]") {
    Transaction t("alice", "bob", 1.0);
    REQUIRE_FALSE(t.isValid());
}

TEST_CASE("restore constructor preserves timestamp so signatures verify",
          "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction original(kf.address(), "receiver", 1.0);
    REQUIRE(original.signTransaction(kf.privHex()));

    Transaction restored(original.getSender(), original.getReceiver(),
                         original.getAmount(), original.getTimestamp(),
                         original.getSignature());
    REQUIRE(restored.getTimestamp() == original.getTimestamp());
    REQUIRE(restored.getSignature() == original.getSignature());
    REQUIRE(restored.calculateHash() == original.calculateHash());
    REQUIRE(restored.verifySignature(kf.pubHex()));
}

TEST_CASE("signTransaction populates sender public key", "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction t(kf.address(), "receiver", 1.0);
    REQUIRE(t.getSenderPublicKey().empty());
    REQUIRE(t.signTransaction(kf.privHex()));
    REQUIRE(t.getSenderPublicKey() == kf.pubHex());
}

TEST_CASE("setSignature stores signature for later retrieval", "[unit][transaction]") {
    test_support::KeyPairFixture kf;
    Transaction original(kf.address(), "receiver", 1.0);
    original.signTransaction(kf.privHex());
    std::string sig = original.getSignature();
    REQUIRE_FALSE(sig.empty());

    Transaction restored(original.getSender(), original.getReceiver(), original.getAmount());
    REQUIRE(restored.getSignature().empty());
    restored.setSignature(sig);
    REQUIRE(restored.getSignature() == sig);
    // Note: verifySignature here would compare against the restored tx's own
    // hash (different timestamp than the original), so signature verification
    // across reconstructed transactions is not expected to succeed until a
    // ctor overload preserves timestamp. See spec §4 bug #1.
}

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
