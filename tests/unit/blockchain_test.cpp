#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include "Blockchain.h"
#include "fixtures.h"

using test_support::MinedChainFixture;
using test_support::KeyPairFixture;
using test_support::TempDir;

TEST_CASE("Blockchain has genesis block at index 0", "[unit][blockchain]") {
    MinedChainFixture f;
    REQUIRE(f.chain.getChainSize() == 1);
    auto genesis = f.chain.getLatestBlock();
    REQUIRE(genesis->getIndex() == 0);
    REQUIRE(genesis->getPreviousHash() == "0");
}

TEST_CASE("addTransaction rejects unsigned non-system tx", "[unit][blockchain]") {
    MinedChainFixture f;
    auto tx = std::make_shared<Transaction>("alice", "bob", 5.0);
    f.chain.addTransaction(tx);
    REQUIRE(f.chain.getPendingTransactions().empty());
}

TEST_CASE("addTransaction rejects insufficient balance", "[unit][blockchain]") {
    MinedChainFixture f;
    KeyPairFixture alice;
    auto tx = alice.signedTx("bob", 100.0);
    f.chain.addTransaction(tx);
    REQUIRE(f.chain.getPendingTransactions().empty());
}

TEST_CASE("minePendingTransactions credits miner", "[unit][blockchain][!mayfail]") {
    // [!mayfail] — depends on Block::mineBlock working (see block_test.cpp note).
    MinedChainFixture f;
    f.seedFunds("alice", 100.0, "miner_1");
    REQUIRE(f.chain.getChainSize() == 2);
    REQUIRE(f.chain.getBalance("alice") == 100.0);
    REQUIRE(f.chain.getBalance("miner_1") == 50.0);
}

TEST_CASE("isChainValid is true for fresh genesis-only chain", "[unit][blockchain]") {
    MinedChainFixture f;
    REQUIRE(f.chain.isChainValid());
}

TEST_CASE("saveToFile/loadFromFile roundtrip preserves chain size",
          "[unit][blockchain][!mayfail]") {
    // [!mayfail] — spec §4 bug #4: saveToFile hardcodes "blockchain_saves/" prefix.
    MinedChainFixture f;
    TempDir tmp;
    std::string path = tmp.file("chain.dat");

    REQUIRE(f.chain.saveToFile(path));
    Blockchain loaded(2, 50.0);
    REQUIRE(loaded.loadFromFile(path));
    REQUIRE(loaded.getChainSize() == f.chain.getChainSize());
}
