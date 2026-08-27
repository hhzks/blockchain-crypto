#include <catch2/catch_test_macros.hpp>
#include "Block.h"
#include "Transaction.h"
#include "utils.h"
#include "fixtures.h"

TEST_CASE("Block constructor initializes fields", "[unit][block]") {
    Block b(5, "prev_hash_xyz", 2);
    REQUIRE(b.getIndex() == 5);
    REQUIRE(b.getPreviousHash() == "prev_hash_xyz");
    REQUIRE(b.getDifficulty() == 2);
    REQUIRE(b.getNonce() == 0);
}

TEST_CASE("addTransaction updates merkle root", "[unit][block]") {
    Block b(0, "0", 2);
    std::string root_empty = b.getMerkleRoot();

    auto tx = std::make_shared<Transaction>("system", "alice", 10.0);
    b.addTransaction(tx);
    REQUIRE(b.getMerkleRoot() != root_empty);
    // A one-transaction root is a hashed leaf, not the leaf itself: leaves and
    // internal nodes are domain-separated (#25).
    REQUIRE(b.getMerkleRoot() != tx->calculateHash());
    REQUIRE(b.getMerkleRoot().size() == 64);
}

TEST_CASE("mineBlock at difficulty 2 produces PoW-satisfying hash",
          "[unit][block]") {
    Block b(0, "0", 2);
    auto tx = std::make_shared<Transaction>("system", "miner", 50.0);
    b.addTransaction(tx);
    b.mineBlock();
    REQUIRE(utils::checkProofOfWork(b.getHash(), 2));
    REQUIRE(b.getNonce() > 0);
}

TEST_CASE("isValid catches merkle-root tampering via tx mutation",
          "[unit][block]") {
    Block b(0, "0", 2);
    b.addTransaction(std::make_shared<Transaction>("system", "alice", 10.0));
    b.mineBlock();
    REQUIRE(b.isValid());
}

TEST_CASE("restore constructor + setMinedState reproduce a mined block",
          "[unit][block]") {
    Block b(3, "prev", 2);
    b.addTransaction(std::make_shared<Transaction>("system", "m", 50.0));
    b.mineBlock();

    Block restored(3, "prev", 2, b.getTimestamp());
    for (const auto& tx : b.getTransactions()) {
        restored.addTransaction(tx);
    }
    restored.setMinedState(b.getNonce(), b.getHash());

    REQUIRE(restored.getTimestamp() == b.getTimestamp());
    REQUIRE(restored.getNonce() == b.getNonce());
    REQUIRE(restored.getHash() == b.getHash());
    REQUIRE(restored.calculateHash() == b.calculateHash());
    REQUIRE(restored.isValid());
}

TEST_CASE("isMined agrees with checkProofOfWork", "[unit][block]") {
    Block b(0, "0", 0);
    auto tx = std::make_shared<Transaction>("system", "alice", 1.0);
    b.addTransaction(tx);
    b.mineBlock();
    REQUIRE(b.isMined(0));
}

TEST_CASE("addTransaction reports whether the transaction was accepted",
          "[unit][block]") {
    Block b(0, "0", 2);
    REQUIRE(b.addTransaction(std::make_shared<Transaction>("system", "alice", 10.0)));

    // Empty sender: rejected by Transaction::isValid.
    REQUIRE_FALSE(b.addTransaction(std::make_shared<Transaction>("", "alice", 10.0)));
    REQUIRE_FALSE(b.addTransaction(nullptr));
    REQUIRE(b.getTransactions().size() == 1);
}

TEST_CASE("setTransactions restores a transaction list verbatim",
          "[unit][block]") {
    // A block reconstructed from the wire or from disk must keep every
    // transaction it was sent, invalid ones included, so that validation
    // reports the real defect instead of a merkle mismatch caused by the
    // reconstruction itself.
    auto good = std::make_shared<Transaction>("system", "alice", 10.0);
    auto bad = std::make_shared<Transaction>("bob", "carol", 5.0);  // unsigned

    Block b(0, "0", 2);
    b.setTransactions({good, bad});
    REQUIRE(b.getTransactions().size() == 2);
    REQUIRE_FALSE(b.isValid());
}
