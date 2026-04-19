#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "P2PMessage.h"

using namespace p2p;

TEST_CASE("Message serialize/deserialize roundtrip preserves type and payload",
          "[unit][p2p]") {
    Message original(MessageType::PING, "hello", "node_abc");
    auto bytes = original.serialize();
    Message restored = Message::deserialize(bytes);
    REQUIRE(restored.getType() == MessageType::PING);
    REQUIRE(restored.getPayload() == "hello");
}

TEST_CASE("Message::deserialize rejects truncated header", "[unit][p2p]") {
    std::vector<uint8_t> too_short(5, 0);
    REQUIRE_THROWS_AS(Message::deserialize(too_short), std::runtime_error);
}

TEST_CASE("Message::deserialize rejects bad magic number", "[unit][p2p]") {
    Message m(MessageType::PING, "x");
    auto bytes = m.serialize();
    bytes[0] = 0xDE;
    REQUIRE_THROWS_AS(Message::deserialize(bytes), std::runtime_error);
}

TEST_CASE("Message::deserialize rejects oversize payload declaration",
          "[unit][p2p]") {
    Message m(MessageType::PING, "x");
    auto bytes = m.serialize();
    bytes[5] = 0xFF; bytes[6] = 0xFF; bytes[7] = 0xFF; bytes[8] = 0xFF;
    REQUIRE_THROWS_AS(Message::deserialize(bytes), std::runtime_error);
}

TEST_CASE("Message::deserialize rejects incomplete payload", "[unit][p2p]") {
    Message m(MessageType::NEW_TRANSACTION, "some_payload_data");
    auto bytes = m.serialize();
    bytes.resize(bytes.size() - 5);
    REQUIRE_THROWS_AS(Message::deserialize(bytes), std::runtime_error);
}

TEST_CASE("PeerInfo serialize/deserialize roundtrip", "[unit][p2p]") {
    PeerInfo p{"127.0.0.1", 8333, "nodeid123", 1700000000LL};
    std::string s = p.serialize();
    PeerInfo r = PeerInfo::deserialize(s);
    REQUIRE(r.ip == p.ip);
    REQUIRE(r.port == p.port);
    REQUIRE(r.node_id == p.node_id);
    REQUIRE(r.last_seen == p.last_seen);
}

TEST_CASE("All message factory helpers produce reparseable messages",
          "[unit][p2p]") {
    struct Case { Message msg; MessageType expected; };
    Case cases[] = {
        {Message::createHandshake("nid", 1234, 0), MessageType::HANDSHAKE},
        {Message::createPing("nid"),               MessageType::PING},
        {Message::createPong("nid"),               MessageType::PONG},
        {Message::createGetPeers(),                MessageType::GET_PEERS},
        {Message::createPeers({}),                 MessageType::PEERS},
        {Message::createGetBlocks(0),              MessageType::GET_BLOCKS},
        {Message::createGetBlockHeight(),          MessageType::GET_BLOCK_HEIGHT},
        {Message::createBlockHeight(5),            MessageType::BLOCK_HEIGHT},
        {Message::createNewBlock("blk"),           MessageType::NEW_BLOCK},
        {Message::createNewTransaction("tx"),      MessageType::NEW_TRANSACTION},
        {Message::createError("boom"),             MessageType::ERROR_MSG},
        {Message::createDisconnect("bye"),         MessageType::DISCONNECT},
    };
    for (const auto& c : cases) {
        INFO("type: " << Message::typeToString(c.expected));
        Message r = Message::deserialize(c.msg.serialize());
        REQUIRE(r.getType() == c.expected);
    }
}
