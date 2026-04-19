#pragma once
#include <memory>
#include <filesystem>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <random>
#include <sstream>
#include "Blockchain.h"
#include "Wallet.h"
#include "Transaction.h"
#include "ECCrypto.h"
#include "P2PNode.h"
#include "vectors.h"

namespace test_support {

// Deterministic keypair derived from fixture_priv_hex
struct KeyPairFixture {
    std::unique_ptr<ECCrypto::KeyPair> kp;
    KeyPairFixture() {
        kp = ECCrypto::keyPairFromPrivateKeyHex(test_vectors::fixture_priv_hex);
    }
    const std::string& address() const { return kp->address; }
    const std::string& privHex() const { return kp->private_key_hex; }
    const std::string& pubHex() const { return kp->public_key_hex; }

    std::shared_ptr<Transaction> signedTx(
        const std::string& to, double amount) const
    {
        auto tx = std::make_shared<Transaction>(kp->address, to, amount);
        tx->signTransaction(kp->private_key_hex);
        return tx;
    }
};

// Pre-mined blockchain at low difficulty for fast tests
struct MinedChainFixture {
    Blockchain chain{2, 50.0};
    MinedChainFixture() = default;

    void seedFunds(const std::string& to, double amount, const std::string& miner) {
        auto tx = std::make_shared<Transaction>("system", to, amount);
        chain.addTransaction(tx);
        chain.minePendingTransactions(miner);
    }
};

// Unique temp directory, cleaned up on destruction
struct TempDir {
    std::filesystem::path path;
    TempDir() {
        auto base = std::filesystem::temp_directory_path();
        std::mt19937_64 rng{std::random_device{}()};
        std::ostringstream name;
        name << "blockchain_test_" << std::hex << rng();
        path = base / name.str();
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
    std::string file(const std::string& leaf) const {
        return (path / leaf).string();
    }
};

// Two P2PNodes on loopback, handshake done. 2s timeouts on all waits.
struct LoopbackPeerFixture {
    Blockchain chain_a{2, 50.0};
    Blockchain chain_b{2, 50.0};
    std::unique_ptr<p2p::P2PNode> node_a;
    std::unique_ptr<p2p::P2PNode> node_b;

    std::mutex mu;
    std::condition_variable cv;
    int handshakes_done = 0;
    std::shared_ptr<Transaction> last_tx_on_b;
    std::shared_ptr<Block> last_block_on_b;

    LoopbackPeerFixture() {
        p2p::P2PConfig cfg_a; cfg_a.listen_port = 0; cfg_a.enable_logging = false;
        p2p::P2PConfig cfg_b; cfg_b.listen_port = 0; cfg_b.enable_logging = false;
        node_a = std::make_unique<p2p::P2PNode>(&chain_a, cfg_a);
        node_b = std::make_unique<p2p::P2PNode>(&chain_b, cfg_b);

        p2p::P2PCallbacks cb_a;
        cb_a.onPeerConnected = [this](std::shared_ptr<p2p::Peer>) {
            std::lock_guard<std::mutex> lk(mu); handshakes_done++; cv.notify_all();
        };
        p2p::P2PCallbacks cb_b = cb_a;
        cb_b.onNewTransaction = [this](std::shared_ptr<Transaction> tx) {
            std::lock_guard<std::mutex> lk(mu); last_tx_on_b = tx; cv.notify_all();
        };
        cb_b.onNewBlock = [this](std::shared_ptr<Block> blk) {
            std::lock_guard<std::mutex> lk(mu); last_block_on_b = blk; cv.notify_all();
        };
        node_a->setCallbacks(cb_a);
        node_b->setCallbacks(cb_b);
    }

    bool start() {
        if (!node_a->start() || !node_b->start()) return false;
        uint16_t port_a = node_a->getActualListenPort();
        return node_b->connectToPeer("127.0.0.1", port_a);
    }

    template <typename Pred>
    bool waitFor(Pred p, std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
        std::unique_lock<std::mutex> lk(mu);
        return cv.wait_for(lk, timeout, p);
    }

    ~LoopbackPeerFixture() {
        if (node_a) node_a->stop();
        if (node_b) node_b->stop();
    }
};

} // namespace test_support
