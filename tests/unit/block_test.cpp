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
    REQUIRE(b.getMerkleRoot() == tx->calculateHash());
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
