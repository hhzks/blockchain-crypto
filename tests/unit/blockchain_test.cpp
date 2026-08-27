#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include "money.h"
#include "Blockchain.h"
#include "P2PMessage.h"
#include "utils.h"
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
    auto tx = std::make_shared<Transaction>("alice", "bob", money::coins(5));
    f.chain.addTransaction(tx);
    REQUIRE(f.chain.getPendingTransactions().empty());
}

TEST_CASE("addTransaction rejects insufficient balance", "[unit][blockchain]") {
    MinedChainFixture f;
    KeyPairFixture alice;
    auto tx = alice.signedTx("bob", money::coins(100));
    f.chain.addTransaction(tx);
    REQUIRE(f.chain.getPendingTransactions().empty());
}

TEST_CASE("minePendingTransactions credits miner", "[unit][blockchain]") {
    MinedChainFixture f;
    f.seedFunds("alice", money::coins(100), "miner_1");
    REQUIRE(f.chain.getChainSize() == 2);
    REQUIRE(f.chain.getBalance("alice") == money::coins(100));
    REQUIRE(f.chain.getBalance("miner_1") == money::coins(50));
}

TEST_CASE("isChainValid is true for fresh genesis-only chain", "[unit][blockchain]") {
    MinedChainFixture f;
    REQUIRE(f.chain.isChainValid());
}

TEST_CASE("constructor difficulty controls genesis and mining difficulty",
          "[unit][blockchain]") {
    Blockchain c(3, money::coins(50));
    REQUIRE(c.getLatestBlock()->getDifficulty() == 3);
    REQUIRE(utils::checkProofOfWork(c.getLatestBlock()->getHash(), 3));
    REQUIRE(c.isChainValid());
}

TEST_CASE("mining and validation agree on difficulty across the adjustment interval",
          "[unit][blockchain]") {
    // Mines past DIFFICULTY_ADJUSTMENT_INTERVAL (10 blocks). Mining and
    // validation previously used two different difficulty rules that
    // diverged at height 11, permanently rejecting every block mined
    // from then on.
    MinedChainFixture f;
    for (int i = 1; i <= 11; ++i) {
        f.seedFunds("user_" + std::to_string(i), money::coins(1), "miner_1");
    }
    REQUIRE(f.chain.getChainSize() == 12);
    REQUIRE(f.chain.isChainValid());
}

TEST_CASE("isChainValid accepts blocks mined within the same second",
          "[unit][blockchain]") {
    // Timestamps have second granularity; consecutive fast-mined blocks
    // share a timestamp and must still validate.
    MinedChainFixture f;
    f.seedFunds("alice", money::coins(100), "miner_1");
    f.seedFunds("bob", money::coins(100), "miner_1");
    REQUIRE(f.chain.getChainSize() == 3);
    REQUIRE(f.chain.isChainValid());
}

TEST_CASE("saveToFile/loadFromFile roundtrip preserves chain state",
          "[unit][blockchain]") {
    MinedChainFixture f;
    KeyPairFixture alice;
    f.seedFunds(alice.address(), money::coins(100), "miner_1");
    auto tx = alice.signedTx("bob", money::coins(25));
    f.chain.addTransaction(tx);
    f.chain.minePendingTransactions("miner_1");

    TempDir tmp;
    std::string path = tmp.file("chain.dat");
    REQUIRE(f.chain.saveToFile(path));

    Blockchain loaded(2, money::coins(50));
    REQUIRE(loaded.loadFromFile(path));
    REQUIRE(loaded.getChainSize() == f.chain.getChainSize());

    // Mined state must survive the roundtrip
    auto orig_tip = f.chain.getLatestBlock();
    auto loaded_tip = loaded.getLatestBlock();
    REQUIRE(loaded_tip->getHash() == orig_tip->getHash());
    REQUIRE(loaded_tip->getNonce() == orig_tip->getNonce());
    REQUIRE(loaded_tip->getTimestamp() == orig_tip->getTimestamp());

    // Signatures must survive too, and the loaded chain must validate
    REQUIRE(loaded_tip->getTransactions()[0]->getSignature() == tx->getSignature());
    REQUIRE(loaded.isChainValid());
    REQUIRE(loaded.getBalance(alice.address()) == money::coins(75));
}

TEST_CASE("saveToFile/loadFromFile preserves signed transaction public key",
          "[unit][blockchain]") {
    MinedChainFixture f;
    KeyPairFixture alice;
    f.seedFunds(alice.address(), money::coins(100), "miner_1");
    auto tx = alice.signedTx("bob", money::coins(25));
    f.chain.addTransaction(tx);
    f.chain.minePendingTransactions("miner_1");

    TempDir tmp;
    std::string path = tmp.file("chain_pubkey.dat");
    REQUIRE(f.chain.saveToFile(path));

    Blockchain loaded(2, money::coins(50));
    REQUIRE(loaded.loadFromFile(path));
    auto loaded_tx = loaded.getLatestBlock()->getTransactions()[0];
    REQUIRE(loaded_tx->getSenderPublicKey() == tx->getSenderPublicKey());
}

TEST_CASE("addBlock accepts a valid next block", "[unit][blockchain]") {
    MinedChainFixture f;
    auto tip = f.chain.getLatestBlock();
    int next_index = static_cast<int>(f.chain.getChainSize());
    int required = f.chain.calculateRequiredDifficulty();

    auto block = std::make_shared<Block>(next_index, tip->getHash(), required);
    block->addTransaction(std::make_shared<Transaction>("system", "miner", money::coins(50)));
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
    block->addTransaction(std::make_shared<Transaction>("system", "miner", money::coins(50)));
    block->mineBlock();

    REQUIRE_FALSE(f.chain.addBlock(block));
    REQUIRE(f.chain.getChainSize() == 1);
}

TEST_CASE("addBlock rejects a block at the wrong index", "[unit][blockchain]") {
    MinedChainFixture f;
    auto tip = f.chain.getLatestBlock();
    int required = f.chain.calculateRequiredDifficulty();

    auto block = std::make_shared<Block>(5, tip->getHash(), required);
    block->addTransaction(std::make_shared<Transaction>("system", "miner", money::coins(50)));
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
    block->addTransaction(std::make_shared<Transaction>("system", "miner", money::coins(50)));
    block->mineBlock();

    REQUIRE_FALSE(f.chain.addBlock(block));
    REQUIRE(f.chain.getChainSize() == 1);
}

TEST_CASE("addBlock rejects a block minting extra system rewards",
          "[unit][blockchain]") {
    // A crafted foreign block with more than one system reward transaction is
    // an unlimited-mint attempt: system txs are exempt from signature checks,
    // so each one would otherwise pass per-transaction validation.
    MinedChainFixture f;
    auto tip = f.chain.getLatestBlock();
    int next_index = static_cast<int>(f.chain.getChainSize());
    int required = f.chain.calculateRequiredDifficulty();

    auto block = std::make_shared<Block>(next_index, tip->getHash(), required);
    block->addTransaction(std::make_shared<Transaction>("system", "attacker", money::coins(50)));
    block->addTransaction(std::make_shared<Transaction>("system", "attacker", money::coins(50)));
    block->mineBlock();

    REQUIRE_FALSE(f.chain.addBlock(block));
    REQUIRE(f.chain.getChainSize() == 1);
}

TEST_CASE("addBlock rejects a block with an inflated system reward amount",
          "[unit][blockchain]") {
    // Exactly one system tx, but its amount exceeds the mining reward.
    MinedChainFixture f;  // mining_reward == money::coins(50)
    auto tip = f.chain.getLatestBlock();
    int next_index = static_cast<int>(f.chain.getChainSize());
    int required = f.chain.calculateRequiredDifficulty();

    auto block = std::make_shared<Block>(next_index, tip->getHash(), required);
    block->addTransaction(std::make_shared<Transaction>("system", "attacker", money::coins(999999)));
    block->mineBlock();

    REQUIRE_FALSE(f.chain.addBlock(block));
    REQUIRE(f.chain.getChainSize() == 1);
}

TEST_CASE("minePendingTransactions refuses to mine an unrewarded block",
          "[unit][blockchain]") {
    // "system" -> "system" fails Transaction::isValid (sender == receiver), so
    // the reward transaction is rejected. Mining must abort rather than commit
    // a block that silently carries no reward.
    Blockchain bc(2, money::coins(50));
    bc.addTransaction(std::make_shared<Transaction>("system", "alice", money::coins(10)));
    size_t height_before = bc.getChainSize();

    bc.minePendingTransactions("system");

    REQUIRE(bc.getChainSize() == height_before);
    REQUIRE(bc.getPendingTransactions().size() == 1);
}

TEST_CASE("loadFromFile rejects a malformed file and keeps the live chain",
          "[unit][blockchain]") {
    MinedChainFixture f;
    f.seedFunds("alice", money::coins(100), "miner_1");
    const size_t height = f.chain.getChainSize();
    const std::string tip = f.chain.getLatestBlock()->getHash();

    TempDir tmp;
    const std::string path = tmp.file("junk.dat");
    {
        std::ofstream out(path);
        out << "not a chain at all" << std::endl;
    }

    REQUIRE_FALSE(f.chain.loadFromFile(path));
    // A failed load must not destroy what the node already had.
    REQUIRE(f.chain.getChainSize() == height);
    REQUIRE(f.chain.getLatestBlock()->getHash() == tip);
}

TEST_CASE("loadFromFile rejects a truncated chain file", "[unit][blockchain]") {
    MinedChainFixture f;
    f.seedFunds("alice", money::coins(100), "miner_1");

    TempDir tmp;
    const std::string full = tmp.file("full.dat");
    REQUIRE(f.chain.saveToFile(full));

    std::string contents;
    {
        std::ifstream in(full);
        contents.assign(std::istreambuf_iterator<char>(in),
                        std::istreambuf_iterator<char>());
    }
    const std::string cut = tmp.file("cut.dat");
    {
        std::ofstream out(cut);
        out << contents.substr(0, contents.size() / 2);
    }

    Blockchain loaded(2, money::coins(50));
    REQUIRE_FALSE(loaded.loadFromFile(cut));
}

TEST_CASE("loadFromFile rejects a chain whose blocks do not validate",
          "[unit][blockchain]") {
    MinedChainFixture f;
    f.seedFunds("alice", money::coins(100), "miner_1");

    TempDir tmp;
    const std::string path = tmp.file("tampered.dat");
    REQUIRE(f.chain.saveToFile(path));

    std::string contents;
    {
        std::ifstream in(path);
        contents.assign(std::istreambuf_iterator<char>(in),
                        std::istreambuf_iterator<char>());
    }
    // Corrupt the tip block's stored hash: every field still parses, so only
    // real validation catches it.
    const std::string tip_hash = f.chain.getLatestBlock()->getHash();
    const auto pos = contents.find(tip_hash);
    REQUIRE(pos != std::string::npos);
    contents.replace(pos, 1, contents[pos] == 'a' ? "b" : "a");
    {
        std::ofstream out(path);
        out << contents;
    }

    Blockchain loaded(2, money::coins(50));
    REQUIRE_FALSE(loaded.loadFromFile(path));
}

TEST_CASE("addTransaction counts pending spends against the sender's balance",
          "[unit][blockchain]") {
    MinedChainFixture f;
    KeyPairFixture alice;
    f.seedFunds(alice.address(), money::coins(100), "miner_1");

    // Two 80-coin sends from a 100-coin balance: the second is only affordable
    // if the first one in the pool is ignored.
    REQUIRE(f.chain.getBalance(alice.address()) == money::coins(100));
    f.chain.addTransaction(alice.signedTx("bob", money::coins(80)));
    f.chain.addTransaction(alice.signedTx("carol", money::coins(80)));

    REQUIRE(f.chain.getPendingTransactions().size() == 1);

    f.chain.minePendingTransactions("miner_1");
    REQUIRE(f.chain.getBalance(alice.address()) == money::coins(20));
}

TEST_CASE("addBlock rejects a block replaying an already-mined transaction",
          "[unit][blockchain]") {
    MinedChainFixture f;
    KeyPairFixture alice;
    f.seedFunds(alice.address(), money::coins(100), "miner_1");

    auto tx = alice.signedTx("bob", money::coins(25));
    f.chain.addTransaction(tx);
    f.chain.minePendingTransactions("miner_1");
    const double bob_balance = f.chain.getBalance("bob");

    // The signature still verifies and the proof of work is real, so only a
    // duplicate check keeps the peer from crediting bob twice.
    auto tip = f.chain.getLatestBlock();
    auto replay = std::make_shared<Block>(
        static_cast<int>(f.chain.getChainSize()), tip->getHash(),
        f.chain.calculateRequiredDifficulty());
    replay->addTransaction(tx);
    replay->addTransaction(std::make_shared<Transaction>("system", "miner_2", money::coins(50)));
    replay->mineBlock();

    REQUIRE_FALSE(f.chain.addBlock(replay));
    REQUIRE(f.chain.getBalance("bob") == bob_balance);
}

TEST_CASE("chain and mempool accessors hand back snapshots, not aliases",
          "[unit][blockchain]") {
    // The P2P receiver thread calls addBlock and addTransaction while the CLI
    // thread walks the chain. An accessor returning a reference to the member
    // vector hands that walk an alias into a container being resized -- the
    // reallocation invalidates it mid-loop. Deterministic single-threaded:
    MinedChainFixture f;

    const std::vector<std::shared_ptr<Transaction>>& pending =
        f.chain.getPendingTransactions();
    REQUIRE(pending.empty());
    f.chain.addTransaction(std::make_shared<Transaction>("system", "alice", money::coins(10)));
    REQUIRE(pending.empty());

    const std::vector<std::shared_ptr<Block>>& blocks = f.chain.getChain();
    const size_t height = blocks.size();
    f.chain.minePendingTransactions("miner_1");
    REQUIRE(blocks.size() == height);
}

TEST_CASE("Blockchain tolerates a peer thread writing while the CLI reads",
          "[unit][blockchain]") {
    // A smoke test, not a proof: without a thread sanitiser a data race can
    // run clean. It does catch a reader walking a vector mid-reallocation.
    MinedChainFixture f;
    f.seedFunds("alice", money::coins(100), "miner_1");

    constexpr int rounds = 2000;
    std::atomic<bool> stop{false};
    std::atomic<bool> go{false};

    std::thread peer_thread([&] {
        while (!go.load()) { }
        for (int i = 0; i < rounds && !stop.load(); ++i) {
            f.chain.addTransaction(std::make_shared<Transaction>(
                "system", "receiver_" + std::to_string(i), money::coins(1)));
        }
    });

    go = true;
    for (int i = 0; i < rounds; ++i) {
        auto snapshot = f.chain.getChain();
        REQUIRE(snapshot.size() >= 2);
        auto pending = f.chain.getPendingTransactions();
        REQUIRE(pending.size() <= static_cast<size_t>(rounds));
        (void)f.chain.getBalance("alice");
    }

    stop = true;
    peer_thread.join();
    SUCCEED("no torn reads observed");
}

TEST_CASE("getBalance is callable on a const chain", "[unit][blockchain]") {
    MinedChainFixture f;
    f.seedFunds("alice", money::coins(100), "miner_1");

    const Blockchain& frozen = f.chain;
    REQUIRE(frozen.getBalance("alice") == money::coins(100));
    REQUIRE(frozen.getBalance("nobody") == money::coins(0));
}

TEST_CASE("system is a mint, not an account with a balance",
          "[unit][blockchain]") {
    // getBalance debited every sender including "system", while
    // updateBalances skipped that debit. The two rules only agreed because
    // nothing read the cache; there is one rule now, and this pins it.
    MinedChainFixture f;
    f.seedFunds("alice", money::coins(100), "miner_1");

    REQUIRE(f.chain.getBalance("system") == money::coins(0));
    REQUIRE(f.chain.getBalance("alice") == money::coins(100));
    REQUIRE(f.chain.getBalance("miner_1") == money::coins(50));
}

TEST_CASE("balances stay correct across every path that appends a block",
          "[unit][blockchain]") {
    // Serving getBalance from the cache is only safe if every mutation path
    // refreshes it. Mining, accepting a foreign block, and loading from disk
    // are the three.
    MinedChainFixture f;
    KeyPairFixture alice;
    f.seedFunds(alice.address(), money::coins(100), "miner_1");
    REQUIRE(f.chain.getBalance(alice.address()) == money::coins(100));

    f.chain.addTransaction(alice.signedTx("bob", money::coins(25)));
    f.chain.minePendingTransactions("miner_1");
    REQUIRE(f.chain.getBalance(alice.address()) == money::coins(75));
    REQUIRE(f.chain.getBalance("bob") == money::coins(25));

    auto tip = f.chain.getLatestBlock();
    auto block = std::make_shared<Block>(
        static_cast<int>(f.chain.getChainSize()), tip->getHash(),
        f.chain.calculateRequiredDifficulty());
    block->addTransaction(alice.signedTx("carol", money::coins(7)));
    block->addTransaction(std::make_shared<Transaction>("system", "miner_2", money::coins(50)));
    block->mineBlock();
    REQUIRE(f.chain.addBlock(block));
    REQUIRE(f.chain.getBalance("carol") == money::coins(7));
    REQUIRE(f.chain.getBalance(alice.address()) == money::coins(68));

    TempDir tmp;
    const std::string path = tmp.file("balances.dat");
    REQUIRE(f.chain.saveToFile(path));

    Blockchain loaded(2, money::coins(50));
    REQUIRE(loaded.loadFromFile(path));
    REQUIRE(loaded.getBalance(alice.address()) == money::coins(68));
    REQUIRE(loaded.getBalance("bob") == money::coins(25));
    REQUIRE(loaded.getBalance("carol") == money::coins(7));
}

TEST_CASE("mining an empty pool still produces a rewarded block",
          "[unit][blockchain]") {
    // A fresh chain has nothing pending, so refusing to mine an empty pool
    // left no way to mint a first reward: no address could ever be funded and
    // therefore no transaction could ever be afforded. Miners mine empty
    // blocks; that is how the first coins exist.
    Blockchain bc(2, money::coins(50));
    const size_t height = bc.getChainSize();

    bc.minePendingTransactions("miner_1");

    REQUIRE(bc.getChainSize() == height + 1);
    REQUIRE(bc.getBalance("miner_1") == money::coins(50));
    REQUIRE(bc.isChainValid());
}

TEST_CASE("a fractional mining reward survives the consensus equality check",
          "[unit][blockchain]") {
    // addBlock gates the reward on an exact ==. While amounts were doubles
    // that comparison depended on the block's amount having been produced the
    // same way as the local configuration, having gone out through a fixed
    // decimal rendering and back. Integers make it exact. A fractional reward
    // is a legitimate configuration and is what the issue called out.
    const money::Amount reward = money::COIN / 10;  // 0.1 coin
    Blockchain bc(2, reward);
    REQUIRE(bc.getMiningReward() == reward);

    bc.minePendingTransactions("miner_1");
    REQUIRE(bc.getBalance("miner_1") == reward);

    auto tip = bc.getLatestBlock();
    auto block = std::make_shared<Block>(
        static_cast<int>(bc.getChainSize()), tip->getHash(),
        bc.calculateRequiredDifficulty());
    block->addTransaction(std::make_shared<Transaction>("system", "miner_2", reward));
    block->mineBlock();

    // Straight through the wire format the peer would have used.
    auto restored = p2p::BlockSerializer::deserialize(
        p2p::BlockSerializer::serialize(*block));
    REQUIRE(restored->getTransactions()[0]->getAmount() == reward);
    REQUIRE(bc.addBlock(restored));
    REQUIRE(bc.getBalance("miner_2") == reward);
}

TEST_CASE("amounts near the supply cap survive save and load",
          "[unit][blockchain]") {
    // A fixed eight-decimal rendering could not carry the fractional part of a
    // value this large through a double.
    const money::Amount huge = money::MAX_MONEY - 1;
    Blockchain bc(2, money::coins(50));
    bc.addTransaction(std::make_shared<Transaction>("system", "whale", huge));
    bc.minePendingTransactions("miner_1");
    REQUIRE(bc.getBalance("whale") == huge);

    TempDir tmp;
    const std::string path = tmp.file("huge.dat");
    REQUIRE(bc.saveToFile(path));

    Blockchain loaded(2, money::coins(50));
    REQUIRE(loaded.loadFromFile(path));
    REQUIRE(loaded.getBalance("whale") == huge);
}
