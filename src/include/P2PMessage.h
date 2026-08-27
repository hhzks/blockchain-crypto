#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <format>
#include "Block.h"
#include "utils.h"
#include "Transaction.h"
#include "money.h"

namespace p2p {

// Big-endian wire codecs. These exist because uint8_t integer-promotes to
// *int*, not unsigned int: `byte << 24` shifts a bit into the sign bit of a
// 32-bit int for any byte >= 0x80, which is undefined behaviour on input a
// peer fully controls. Every shift below happens in an unsigned type.
inline uint32_t readU32BE(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
            static_cast<uint32_t>(p[3]);
}

inline uint16_t readU16BE(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint32_t>(p[0]) << 8) |
                                  static_cast<uint32_t>(p[1]));
}

inline int64_t readI64BE(const uint8_t* p) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<uint64_t>(p[i]);
    }
    return static_cast<int64_t>(value);
}

inline void appendU32BE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >>  8) & 0xFF));
    out.push_back(static_cast<uint8_t>( value        & 0xFF));
}

inline void appendU16BE(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>( value       & 0xFF));
}

inline void appendI64BE(std::vector<uint8_t>& out, int64_t value) {
    const uint64_t bits = static_cast<uint64_t>(value);
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<uint8_t>((bits >> (i * 8)) & 0xFF));
    }
}

enum class MessageType : uint8_t {
    HANDSHAKE = 0x00,
    HANDSHAKE_ACK = 0x01,
    
    PING = 0x10,
    PONG = 0x11,
    GET_PEERS = 0x12,
    PEERS = 0x13,
    
    GET_BLOCKS = 0x20,
    BLOCKS = 0x21,
    GET_BLOCK_HEIGHT = 0x22,
    BLOCK_HEIGHT = 0x23,
    
    NEW_BLOCK = 0x30,
    NEW_TRANSACTION = 0x31,
    
    ERROR_MSG = 0xFE,
    DISCONNECT = 0xFF
};

struct PeerInfo {
    std::string ip;
    uint16_t port;
    std::string node_id;
    int64_t last_seen;
    
    std::string serialize() const {
        return ip + ":" + std::to_string(port) + ":" + node_id + ":" + std::to_string(last_seen);
    }
    
    // Throws std::invalid_argument on malformed input; callers parsing
    // peer-supplied data must catch it (see P2PNode::handlePeers).
    static PeerInfo deserialize(const std::string& data) {
        PeerInfo info;
        std::istringstream iss(data);
        std::string token;
        
        std::getline(iss, info.ip, ':');
        std::getline(iss, token, ':');
        auto port = utils::parseInt(token);
        if (!port || *port <= 0 || *port > 65535) {
            throw std::invalid_argument("PeerInfo: invalid port '" + token + "'");
        }
        info.port = static_cast<uint16_t>(*port);

        std::getline(iss, info.node_id, ':');
        std::getline(iss, token, ':');
        auto last_seen = utils::parseInt64(token);
        if (!last_seen) {
            throw std::invalid_argument("PeerInfo: invalid last_seen '" + token + "'");
        }
        info.last_seen = *last_seen;
        
        return info;
    }
};

class Message {
private:
    MessageType type;
    std::string payload;
    std::string sender_id;
    int64_t timestamp;
    
public:
    // Wire frame, big-endian throughout:
    //   magic(4) type(1) payload_len(4) timestamp(8) sender_len(2)
    //   sender_id(sender_len) payload(payload_len)
    // HEADER_SIZE is the fixed prefix only; a frame is HEADER_SIZE plus the
    // two length-prefixed variable fields. Readers must consume both.
    static constexpr uint32_t MAGIC_NUMBER = 0x424C4B43; // "BLKC"
    static constexpr size_t HEADER_SIZE = 19;
    static constexpr size_t MAX_PAYLOAD_SIZE = 10 * 1024 * 1024;
    // Bounded by the 2-byte length prefix that carries it.
    static constexpr size_t MAX_SENDER_ID_SIZE = 0xFFFF;
    
    Message() : type(MessageType::PING), timestamp(0) {}
    
    Message(MessageType t, const std::string& data = "", const std::string& sender = "")
        : type(t), payload(data), sender_id(sender) {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    MessageType getType() const { return type; }
    const std::string& getPayload() const { return payload; }
    const std::string& getSenderId() const { return sender_id; }
    int64_t getTimestamp() const { return timestamp; }
    
    void setType(MessageType t) { type = t; }
    void setPayload(const std::string& data) { payload = data; }
    void setSenderId(const std::string& id) { sender_id = id; }
    
    std::vector<uint8_t> serialize() const {
        if (sender_id.size() > MAX_SENDER_ID_SIZE) {
            throw std::runtime_error("Invalid message: sender id too long");
        }

        std::vector<uint8_t> result;

        // Magic number (4 bytes)
        appendU32BE(result, MAGIC_NUMBER);

        // Message type (1 byte)
        result.push_back(static_cast<uint8_t>(type));

        // Payload length (4 bytes)
        appendU32BE(result, static_cast<uint32_t>(payload.size()));

        // Timestamp (8 bytes)
        appendI64BE(result, timestamp);

        // Sender id length (2 bytes)
        appendU16BE(result, static_cast<uint16_t>(sender_id.size()));

        // Sender id
        for (char c : sender_id) {
            result.push_back(static_cast<uint8_t>(c));
        }

        // Payload
        for (char c : payload) {
            result.push_back(static_cast<uint8_t>(c));
        }

        return result;
    }
    
    static Message deserialize(const std::vector<uint8_t>& data) {
        Message msg;
        
        if (data.size() < HEADER_SIZE) {
            throw std::runtime_error("Invalid message: too short");
        }
        
        // Verify magic number
        const uint32_t magic = readU32BE(data.data());
        if (magic != MAGIC_NUMBER) {
            throw std::runtime_error("Invalid message: bad magic number");
        }

        // Message type
        msg.type = static_cast<MessageType>(data[4]);

        // Payload length
        const uint32_t len = readU32BE(data.data() + 5);
        if (len > MAX_PAYLOAD_SIZE) {
            throw std::runtime_error("Invalid message: payload too large");
        }

        // Timestamp
        msg.timestamp = readI64BE(data.data() + 9);

        // Sender id
        const uint16_t sender_len = readU16BE(data.data() + 17);
        if (data.size() < HEADER_SIZE + sender_len) {
            throw std::runtime_error("Invalid message: incomplete sender id");
        }
        msg.sender_id = std::string(data.begin() + HEADER_SIZE,
                                    data.begin() + HEADER_SIZE + sender_len);

        // Payload
        const size_t payload_start = HEADER_SIZE + sender_len;
        if (data.size() < payload_start + len) {
            throw std::runtime_error("Invalid message: incomplete payload");
        }
        msg.payload = std::string(data.begin() + payload_start,
                                  data.begin() + payload_start + len);

        return msg;
    }
    
    static Message createHandshake(const std::string& node_id, uint16_t port, int64_t block_height) {
        std::ostringstream oss;
        oss << node_id << "|" << port << "|" << block_height << "|1.0.0";
        return Message(MessageType::HANDSHAKE, oss.str());
    }
    
    static Message createPing(const std::string& node_id) {
        return Message(MessageType::PING, node_id);
    }
    
    static Message createPong(const std::string& node_id) {
        return Message(MessageType::PONG, node_id);
    }
    
    static Message createGetPeers() {
        return Message(MessageType::GET_PEERS, "");
    }
    
    static Message createPeers(const std::vector<PeerInfo>& peers_list) {
        std::ostringstream oss;
        for (size_t i = 0; i < peers_list.size(); ++i) {
            if (i > 0) oss << "\n";
            oss << peers_list[i].serialize();
        }
        return Message(MessageType::PEERS, oss.str());
    }
    
    static Message createGetBlocks(int64_t start_height, int64_t end_height = -1) {
        std::ostringstream oss;
        oss << start_height << "|" << end_height;
        return Message(MessageType::GET_BLOCKS, oss.str());
    }
    
    static Message createGetBlockHeight() {
        return Message(MessageType::GET_BLOCK_HEIGHT, "");
    }
    
    static Message createBlockHeight(int64_t height) {
        return Message(MessageType::BLOCK_HEIGHT, std::to_string(height));
    }
    
    static Message createNewBlock(const std::string& serialized_block) {
        return Message(MessageType::NEW_BLOCK, serialized_block);
    }
    
    static Message createNewTransaction(const std::string& serialized_tx) {
        return Message(MessageType::NEW_TRANSACTION, serialized_tx);
    }
    
    static Message createError(const std::string& error_msg) {
        return Message(MessageType::ERROR_MSG, error_msg);
    }
    
    static Message createDisconnect(const std::string& reason = "") {
        return Message(MessageType::DISCONNECT, reason);
    }
    
    static std::string typeToString(MessageType t) {
        switch (t) {
            case MessageType::HANDSHAKE: return "HANDSHAKE";
            case MessageType::HANDSHAKE_ACK: return "HANDSHAKE_ACK";
            case MessageType::PING: return "PING";
            case MessageType::PONG: return "PONG";
            case MessageType::GET_PEERS: return "GET_PEERS";
            case MessageType::PEERS: return "PEERS";
            case MessageType::GET_BLOCKS: return "GET_BLOCKS";
            case MessageType::BLOCKS: return "BLOCKS";
            case MessageType::GET_BLOCK_HEIGHT: return "GET_BLOCK_HEIGHT";
            case MessageType::BLOCK_HEIGHT: return "BLOCK_HEIGHT";
            case MessageType::NEW_BLOCK: return "NEW_BLOCK";
            case MessageType::NEW_TRANSACTION: return "NEW_TRANSACTION";
            case MessageType::ERROR_MSG: return "ERROR";
            case MessageType::DISCONNECT: return "DISCONNECT";
            default: return "UNKNOWN";
        }
    }
};

class BlockSerializer {
public:
    static std::string serialize(const Block& block) {
        std::ostringstream oss;
        
        oss << block.getIndex() << "|"
            << block.getTimestamp() << "|"
            << block.getPreviousHash() << "|"
            << block.getHash() << "|"
            << block.getMerkleRoot() << "|"
            << block.getDifficulty() << "|"
            << block.getNonce() << "|";
        
        const auto& txs = block.getTransactions();
        oss << txs.size();
        
        for (const auto& tx : txs) {
            oss << "|" << tx->getSender()
                << "," << tx->getReceiver()
                << "," << tx->getAmount()
                << "," << tx->getTimestamp()
                << "," << tx->getSignature()
                << "," << tx->getSenderPublicKey()
                << "," << tx->getNonce();
        }

        return oss.str();
    }

    static std::shared_ptr<Block> deserialize(const std::string& data) {
        std::istringstream iss(data);
        std::string token;

        int index;
        long long timestamp;
        std::string prev_hash, hash, merkle_root;
        int difficulty, nonce;
        size_t tx_count;

        std::getline(iss, token, '|'); index = std::stoi(token);
        std::getline(iss, token, '|'); timestamp = std::stoll(token);
        std::getline(iss, prev_hash, '|');
        std::getline(iss, hash, '|');
        std::getline(iss, merkle_root, '|');
        std::getline(iss, token, '|'); difficulty = std::stoi(token);
        std::getline(iss, token, '|'); nonce = std::stoi(token);
        std::getline(iss, token, '|'); tx_count = std::stoull(token);

        auto block = std::make_shared<Block>(index, prev_hash, difficulty, timestamp);

        std::vector<std::shared_ptr<Transaction>> txs;
        txs.reserve(tx_count);

        for (size_t i = 0; i < tx_count; ++i) {
            std::string tx_data;
            std::getline(iss, tx_data, '|');

            std::istringstream tx_stream(tx_data);
            std::string sender, receiver, sig, pubkey;
            money::Amount amount;
            long long tx_timestamp;
            std::uint64_t tx_nonce;

            std::getline(tx_stream, sender, ',');
            std::getline(tx_stream, receiver, ',');
            std::getline(tx_stream, token, ','); amount = std::stoll(token);
            std::getline(tx_stream, token, ','); tx_timestamp = std::stoll(token);
            std::getline(tx_stream, sig, ',');
            std::getline(tx_stream, pubkey, ',');
            std::getline(tx_stream, token, ','); tx_nonce = std::stoull(token);

            auto tx = std::make_shared<Transaction>(sender, receiver, amount,
                                                    tx_timestamp, sig, tx_nonce);
            tx->setSenderPublicKey(pubkey);
            txs.push_back(tx);
        }

        // Rebuild the peer's block exactly as sent; Block::isValid() decides
        // whether it is acceptable.
        block->setTransactions(std::move(txs));
        block->setMinedState(nonce, hash);
        return block;
    }
};

class TransactionSerializer {
public:
    static std::string serialize(const Transaction& tx) {
        std::ostringstream oss;
        oss << tx.getSender() << "|"
            << tx.getReceiver() << "|"
            << tx.getAmount() << "|"
            << tx.getTimestamp() << "|"
            << tx.getSignature() << "|"
            << tx.getSenderPublicKey() << "|"
            << tx.getNonce();
        return oss.str();
    }

    static std::shared_ptr<Transaction> deserialize(const std::string& data) {
        std::istringstream iss(data);
        std::string sender, receiver, sig, pubkey, token;
        money::Amount amount;
        long long timestamp;
        std::uint64_t nonce;

        std::getline(iss, sender, '|');
        std::getline(iss, receiver, '|');
        std::getline(iss, token, '|'); amount = std::stoll(token);
        std::getline(iss, token, '|'); timestamp = std::stoll(token);
        std::getline(iss, sig, '|');
        std::getline(iss, pubkey, '|');
        std::getline(iss, token, '|'); nonce = std::stoull(token);

        auto tx = std::make_shared<Transaction>(sender, receiver, amount,
                                                timestamp, sig, nonce);
        tx->setSenderPublicKey(pubkey);
        return tx;
    }
};

}
