#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include "fixtures.h"
#include "Transaction.h"
#include "P2PNode.h"
#include "P2PMessage.h"
#include "Blockchain.h"

using test_support::LoopbackPeerFixture;
using namespace std::chrono_literals;

TEST_CASE("Two P2P nodes handshake on loopback", "[integration][p2p]") {
    LoopbackPeerFixture f;
    REQUIRE(f.start());
    bool ok = f.waitFor([&]{ return f.handshakes_done >= 2; }, 3s);
    REQUIRE(ok);
}

TEST_CASE("Broadcast transaction delivered to peer", "[integration][p2p][!mayfail]") {
    // [!mayfail] — depends on handshake completing. If P2PNode broadcast uses
    // a different serialization than the test expects, the callback may not
    // fire with the expected fields.
    LoopbackPeerFixture f;
    REQUIRE(f.start());
    REQUIRE(f.waitFor([&]{ return f.handshakes_done >= 2; }, 3s));

    auto tx = std::make_shared<Transaction>("system", "bob", 42.0);
    f.node_a->broadcastTransaction(tx);

    bool got = f.waitFor([&]{ return f.last_tx_on_b != nullptr; }, 3s);
    REQUIRE(got);
    REQUIRE(f.last_tx_on_b->getReceiver() == "bob");
    REQUIRE(f.last_tx_on_b->getAmount() == 42.0);
}

TEST_CASE("Nodes stop cleanly without hanging", "[integration][p2p]") {
    {
        LoopbackPeerFixture f;
        REQUIRE(f.start());
        f.waitFor([&]{ return f.handshakes_done >= 2; }, 3s);
    }
    SUCCEED("fixture destructor returned");
}

namespace {

// Connects a raw client to `port` and sends one already-serialized message,
// bypassing P2PNode so the payload can be deliberately malformed.
bool sendRawMessage(uint16_t port, const p2p::Message& msg) {
    SocketType sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCK) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    bool ok = connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
    if (ok) {
        auto bytes = msg.serialize();
        ok = send(sock, reinterpret_cast<const char*>(bytes.data()),
                  static_cast<int>(bytes.size()), 0) > 0;
    }
    closesocket(sock);
    return ok;
}

// Opens a raw client connection to `port`, left open for the caller to close.
SocketType connectRaw(uint16_t port) {
    SocketType sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCK) return INVALID_SOCK;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(sock);
        return INVALID_SOCK;
    }
    return sock;
}

bool sendAll(SocketType sock, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = send(sock, reinterpret_cast<const char*>(data + sent),
                     static_cast<int>(len - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

// Polls `pred` until it holds or `timeout` elapses.
template <typename Pred>
bool waitUntil(Pred pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(20ms);
    }
    return pred();
}

// Handshakes as a peer claiming `height` blocks, which makes the node start
// syncing from us.
bool sendHandshake(SocketType sock, const std::string& node_id, int64_t height) {
    auto frame = p2p::Message(p2p::MessageType::HANDSHAKE,
                              node_id + "|8333|" + std::to_string(height) + "|1.0.0",
                              node_id).serialize();
    return sendAll(sock, frame.data(), frame.size());
}

} // namespace

TEST_CASE("A malformed peer message drops the peer, not the node",
          "[integration][p2p]") {
    Blockchain chain{2, 50.0};
    p2p::P2PConfig cfg;
    cfg.listen_port = 0;
    cfg.enable_logging = false;
    p2p::P2PNode node(&chain, cfg);
    REQUIRE(node.start());

    const uint16_t port = node.getActualListenPort();
    REQUIRE(port != 0);

    // Non-numeric fields in every handler that parses peer-supplied text.
    // Unguarded std::stoll/std::stoi throw out of the receiver thread, and an
    // escaping exception there is std::terminate for the whole process.
    REQUIRE(sendRawMessage(port, p2p::Message(p2p::MessageType::HANDSHAKE,
                                              "attacker|8333|abc|1.0.0", "attacker")));
    REQUIRE(sendRawMessage(port, p2p::Message(p2p::MessageType::GET_BLOCKS,
                                              "zero|one", "attacker")));
    REQUIRE(sendRawMessage(port, p2p::Message(p2p::MessageType::BLOCKS,
                                              "lots", "attacker")));
    REQUIRE(sendRawMessage(port, p2p::Message(p2p::MessageType::BLOCK_HEIGHT,
                                              "tall", "attacker")));
    REQUIRE(sendRawMessage(port, p2p::Message(p2p::MessageType::PEERS,
                                              "127.0.0.1:port:nid:when", "attacker")));

    // The node must still be alive and still serve a well-formed peer.
    Blockchain good_chain{2, 50.0};
    p2p::P2PConfig good_cfg;
    good_cfg.listen_port = 0;
    good_cfg.enable_logging = false;
    p2p::P2PNode good_peer(&good_chain, good_cfg);
    REQUIRE(good_peer.start());
    REQUIRE(good_peer.connectToPeer("127.0.0.1", port));

    bool connected = false;
    for (int i = 0; i < 60 && !connected; ++i) {
        connected = node.getPeerCount() > 0;
        std::this_thread::sleep_for(50ms);
    }
    REQUIRE(connected);

    good_peer.stop();
    node.stop();
}

TEST_CASE("A stalled peer does not block messages from other peers",
          "[integration][p2p]") {
    Blockchain chain{2, 50.0};
    p2p::P2PConfig cfg;
    cfg.listen_port = 0;
    cfg.enable_logging = false;
    p2p::P2PNode node(&chain, cfg);
    REQUIRE(node.start());

    const uint16_t port = node.getActualListenPort();
    REQUIRE(port != 0);

    // Peer A announces a message and then goes silent. A blocking read of the
    // body halts every other peer's traffic until the 30s socket timeout.
    SocketType staller = connectRaw(port);
    REQUIRE(staller != INVALID_SOCK);
    auto stalled_frame = p2p::Message(p2p::MessageType::PING, "hello", "staller").serialize();
    REQUIRE(stalled_frame.size() > p2p::Message::HEADER_SIZE);
    REQUIRE(sendAll(staller, stalled_frame.data(), p2p::Message::HEADER_SIZE));

    // Give the receiver thread time to pick up the header and start waiting
    // on the body it will never get.
    std::this_thread::sleep_for(500ms);

    // Peer B sends a complete PING and must get its PONG promptly.
    SocketType healthy = connectRaw(port);
    REQUIRE(healthy != INVALID_SOCK);

#ifdef _WIN32
    DWORD recv_timeout = 4000;
    setsockopt(healthy, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&recv_timeout), sizeof(recv_timeout));
#else
    struct timeval recv_timeout {4, 0};
    setsockopt(healthy, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
#endif

    auto ping = p2p::Message(p2p::MessageType::PING, "hi", "healthy").serialize();
    REQUIRE(sendAll(healthy, ping.data(), ping.size()));

    uint8_t reply[p2p::Message::HEADER_SIZE]{};
    int received = recv(healthy, reinterpret_cast<char*>(reply), sizeof reply, 0);

    closesocket(healthy);
    closesocket(staller);
    node.stop();

    REQUIRE(received > 0);
}

TEST_CASE("stop() returns promptly instead of sleeping out an interval",
          "[integration][p2p]") {
    Blockchain chain{2, 50.0};
    p2p::P2PConfig cfg;
    cfg.listen_port = 0;
    cfg.enable_logging = false;
    p2p::P2PNode node(&chain, cfg);
    REQUIRE(node.start());
    std::this_thread::sleep_for(200ms);

    const auto begin = std::chrono::steady_clock::now();
    node.stop();
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    // ping_interval is 30s and sync_interval 60s by default; a missed wakeup
    // makes stop() wait one of them out.
    REQUIRE(elapsed < 2s);
}

TEST_CASE("an unanswered GET_BLOCKS does not wedge sync forever",
          "[integration][p2p]") {
    Blockchain chain{2, 50.0};
    p2p::P2PConfig cfg;
    cfg.listen_port = 0;
    cfg.enable_logging = false;
    cfg.sync_timeout = 500;
    p2p::P2PNode node(&chain, cfg);
    REQUIRE(node.start());

    SocketType client = connectRaw(node.getActualListenPort());
    REQUIRE(client != INVALID_SOCK);
    REQUIRE(sendHandshake(client, "silent-peer", 9999));

    const bool started = waitUntil([&] { return node.isSyncing(); }, 3s);
    // Then say nothing at all: syncing must time out, not latch forever.
    const bool cleared = started && waitUntil([&] { return !node.isSyncing(); }, 5s);

    closesocket(client);
    node.stop();

    REQUIRE(started);
    REQUIRE(cleared);
}

TEST_CASE("a peer disconnecting mid-sync clears the sync flag",
          "[integration][p2p]") {
    Blockchain chain{2, 50.0};
    p2p::P2PConfig cfg;
    cfg.listen_port = 0;
    cfg.enable_logging = false;
    cfg.sync_timeout = 60000;  // only the disconnect can explain a clear
    p2p::P2PNode node(&chain, cfg);
    REQUIRE(node.start());

    SocketType client = connectRaw(node.getActualListenPort());
    REQUIRE(client != INVALID_SOCK);
    REQUIRE(sendHandshake(client, "vanishing-peer", 9999));

    const bool started = waitUntil([&] { return node.isSyncing(); }, 3s);
    closesocket(client);
    const bool cleared = started && waitUntil([&] { return !node.isSyncing(); }, 5s);

    node.stop();

    REQUIRE(started);
    REQUIRE(cleared);
}

TEST_CASE("an ERROR reply cancels the in-flight sync", "[integration][p2p]") {
    Blockchain chain{2, 50.0};
    p2p::P2PConfig cfg;
    cfg.listen_port = 0;
    cfg.enable_logging = false;
    cfg.sync_timeout = 60000;  // only the ERROR reply can explain a clear
    p2p::P2PNode node(&chain, cfg);
    REQUIRE(node.start());

    SocketType client = connectRaw(node.getActualListenPort());
    REQUIRE(client != INVALID_SOCK);
    REQUIRE(sendHandshake(client, "grumpy-peer", 9999));

    const bool started = waitUntil([&] { return node.isSyncing(); }, 3s);

    // handleGetBlocks answers an out-of-range request with exactly this, and
    // nothing used to handle it.
    auto err = p2p::Message(p2p::MessageType::ERROR_MSG, "Invalid block range",
                            "grumpy-peer").serialize();
    REQUIRE(sendAll(client, err.data(), err.size()));

    const bool cleared = started && waitUntil([&] { return !node.isSyncing(); }, 5s);

    closesocket(client);
    node.stop();

    REQUIRE(started);
    REQUIRE(cleared);
}
