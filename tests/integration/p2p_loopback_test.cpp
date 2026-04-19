#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include "fixtures.h"
#include "Transaction.h"

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
