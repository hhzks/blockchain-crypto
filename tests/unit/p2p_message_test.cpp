#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <limits>
#include "P2PMessage.h"
#include "Block.h"
#include "Transaction.h"
#include "fixtures.h"

using namespace p2p;

TEST_CASE("Message serialize/deserialize roundtrip preserves type and payload",
          "[unit][p2p]") {
    Message original(MessageType::PING, "hello", "node_abc");
    auto bytes = original.serialize();
    Message restored = Message::deserialize(bytes);
    REQUIRE(restored.getType() == MessageType::PING);
    REQUIRE(restored.getPayload() == "hello");
    REQUIRE(restored.getSenderId() == "node_abc");
}

TEST_CASE("Message roundtrip preserves sender id independently of payload",
          "[unit][p2p]") {
    Message original(MessageType::NEW_BLOCK, "block_data", "sender-abc");
    Message restored = Message::deserialize(original.serialize());
    REQUIRE(restored.getSenderId() == "sender-abc");
    REQUIRE(restored.getPayload() == "block_data");
}

TEST_CASE("Message roundtrip preserves a sender id set after construction",
          "[unit][p2p]") {
    Message original(MessageType::PONG, "payload");
    original.setSenderId("late-sender");
    Message restored = Message::deserialize(original.serialize());
    REQUIRE(restored.getSenderId() == "late-sender");
}

TEST_CASE("Message roundtrip preserves an empty sender id", "[unit][p2p]") {
    Message original(MessageType::GET_PEERS, "");
    Message restored = Message::deserialize(original.serialize());
    REQUIRE(restored.getSenderId().empty());
    REQUIRE(restored.getPayload().empty());
}

TEST_CASE("Message roundtrip preserves a sender id holding delimiter bytes",
          "[unit][p2p]") {
    // The frame is length-prefixed, not delimited, so these must survive.
    const std::string awkward("a|b:c\nd\0e", 9);
    Message original(MessageType::PING, "payload", awkward);
    Message restored = Message::deserialize(original.serialize());
    REQUIRE(restored.getSenderId() == awkward);
    REQUIRE(restored.getPayload() == "payload");
}

TEST_CASE("Message::deserialize rejects incomplete sender id", "[unit][p2p]") {
    Message m(MessageType::PING, "", "a_long_sender_identifier");
    auto bytes = m.serialize();
    bytes.resize(bytes.size() - 4);
    REQUIRE_THROWS_AS(Message::deserialize(bytes), std::runtime_error);
}

TEST_CASE("Message::deserialize rejects a payload truncated by the sender id",
          "[unit][p2p]") {
    // Declared sender id fits, but it pushes the payload past the buffer end.
    Message m(MessageType::PING, "payload_here", "sender");
    auto bytes = m.serialize();
    bytes.resize(bytes.size() - 3);
    REQUIRE_THROWS_AS(Message::deserialize(bytes), std::runtime_error);
}

TEST_CASE("Message::serialize rejects a sender id too long to frame",
          "[unit][p2p]") {
    Message m(MessageType::PING, "x",
              std::string(Message::MAX_SENDER_ID_SIZE + 1, 'n'));
    REQUIRE_THROWS_AS(m.serialize(), std::runtime_error);
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

TEST_CASE("TransactionSerializer roundtrip preserves timestamp and signature",
          "[unit][p2p]") {
    test_support::KeyPairFixture kf;
    auto tx = kf.signedTx("bob", 3.5);

    auto restored = TransactionSerializer::deserialize(
        TransactionSerializer::serialize(*tx));
    REQUIRE(restored->getTimestamp() == tx->getTimestamp());
    REQUIRE(restored->getSignature() == tx->getSignature());
    REQUIRE(restored->calculateHash() == tx->calculateHash());
}

TEST_CASE("BlockSerializer roundtrip preserves mined state", "[unit][p2p]") {
    Block b(1, "prevhash", 2);
    b.addTransaction(std::make_shared<Transaction>("system", "miner", 50.0));
    b.mineBlock();

    auto restored = BlockSerializer::deserialize(BlockSerializer::serialize(b));
    REQUIRE(restored->getTimestamp() == b.getTimestamp());
    REQUIRE(restored->getNonce() == b.getNonce());
    REQUIRE(restored->getHash() == b.getHash());
    REQUIRE(restored->calculateHash() == b.calculateHash());
    REQUIRE(restored->isValid());
}

TEST_CASE("TransactionSerializer preserves sender public key", "[unit][p2p]") {
    test_support::KeyPairFixture kf;
    auto tx = kf.signedTx("bob", 3.5);
    auto restored = TransactionSerializer::deserialize(
        TransactionSerializer::serialize(*tx));
    REQUIRE(restored->getSenderPublicKey() == kf.pubHex());
    REQUIRE(restored->getSignature() == tx->getSignature());
}

TEST_CASE("BlockSerializer preserves signed transaction public key",
          "[unit][p2p]") {
    test_support::KeyPairFixture kf;
    Block b(1, "prevhash", 2);
    b.addTransaction(kf.signedTx("bob", 5.0));
    b.mineBlock();

    auto restored = BlockSerializer::deserialize(BlockSerializer::serialize(b));
    REQUIRE(restored->getTransactions().size() == 1);
    REQUIRE(restored->getTransactions()[0]->getSenderPublicKey() == kf.pubHex());
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

// Issue #16: the header decode shifted uint8_t values left by 24, and folded
// bytes into an int64_t accumulator. uint8_t integer-promotes to int, not
// unsigned int, so any byte >= 0x80 shifted a bit into the sign bit of a 32-bit
// int -- undefined behaviour on attacker-controlled input.
TEST_CASE("Message::deserialize decodes an all-ones timestamp as -1",
          "[unit][p2p]") {
    std::vector<uint8_t> frame(Message::HEADER_SIZE, 0);
    frame[0] = 0x42; frame[1] = 0x4C; frame[2] = 0x4B; frame[3] = 0x43; // "BLKC"
    frame[4] = static_cast<uint8_t>(MessageType::PING);
    for (int i = 0; i < 8; ++i) frame[9 + i] = 0xFF; // timestamp bytes

    const Message restored = Message::deserialize(frame);
    REQUIRE(restored.getTimestamp() == -1);
}

TEST_CASE("Message::deserialize decodes a high-bit timestamp as INT64_MIN",
          "[unit][p2p]") {
    std::vector<uint8_t> frame(Message::HEADER_SIZE, 0);
    frame[0] = 0x42; frame[1] = 0x4C; frame[2] = 0x4B; frame[3] = 0x43;
    frame[4] = static_cast<uint8_t>(MessageType::PING);
    frame[9] = 0x80; // top bit of the timestamp only

    const Message restored = Message::deserialize(frame);
    REQUIRE(restored.getTimestamp() == std::numeric_limits<int64_t>::min());
}

TEST_CASE("Message::deserialize rejects a high-bit magic number", "[unit][p2p]") {
    std::vector<uint8_t> frame(Message::HEADER_SIZE, 0);
    frame[0] = 0xC2; frame[1] = 0x4C; frame[2] = 0x4B; frame[3] = 0x43;
    REQUIRE_THROWS_AS(Message::deserialize(frame), std::runtime_error);
}

TEST_CASE("Message::deserialize rejects a high-bit payload length", "[unit][p2p]") {
    std::vector<uint8_t> frame(Message::HEADER_SIZE, 0);
    frame[0] = 0x42; frame[1] = 0x4C; frame[2] = 0x4B; frame[3] = 0x43;
    frame[4] = static_cast<uint8_t>(MessageType::PING);
    frame[5] = 0xFF; frame[6] = 0xFF; frame[7] = 0xFF; frame[8] = 0xFF;
    REQUIRE_THROWS_AS(Message::deserialize(frame), std::runtime_error);
}

TEST_CASE("BlockSerializer::deserialize keeps a block's invalid transactions",
          "[unit][p2p]") {
    test_support::KeyPairFixture kf;
    Block b(1, "prevhash", 2);
    b.addTransaction(kf.signedTx("bob", 5.0));
    b.addTransaction(std::make_shared<Transaction>("system", "miner", 50.0));
    b.mineBlock();

    // Corrupt the signed transaction's signature on the wire: the peer sent
    // two transactions, so two must come back, and the block must be reported
    // invalid rather than silently reshaped into a one-transaction block.
    std::string wire = BlockSerializer::serialize(b);
    const std::string& sig = b.getTransactions()[0]->getSignature();
    auto pos = wire.find(sig);
    REQUIRE(pos != std::string::npos);
    wire.replace(pos, 1, wire[pos] == 'a' ? "b" : "a");

    auto restored = BlockSerializer::deserialize(wire);
    REQUIRE(restored->getTransactions().size() == 2);
    REQUIRE_FALSE(restored->isValid());
}
