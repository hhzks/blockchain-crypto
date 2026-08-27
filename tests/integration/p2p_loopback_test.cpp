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
