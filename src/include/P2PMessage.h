#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <memory>
#include "Block.h"
#include "Transaction.h"

namespace p2p {

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
    
    static PeerInfo deserialize(const std::string& data) {
        PeerInfo info;
        std::istringstream iss(data);
        std::string token;
        
        std::getline(iss, info.ip, ':');
        std::getline(iss, token, ':');
        info.port = static_cast<uint16_t>(std::stoi(token));
        std::getline(iss, info.node_id, ':');
        std::getline(iss, token, ':');
        info.last_seen = std::stoll(token);
        
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
    static constexpr uint32_t MAGIC_NUMBER = 0x424C4B43; // "BLKC"
    static constexpr size_t HEADER_SIZE = 17;
    static constexpr size_t MAX_PAYLOAD_SIZE = 10 * 1024 * 1024;
    
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
        std::vector<uint8_t> result;
        
        // Magic number (4 bytes)
        result.push_back((MAGIC_NUMBER >> 24) & 0xFF);
        result.push_back((MAGIC_NUMBER >> 16) & 0xFF);
        result.push_back((MAGIC_NUMBER >> 8) & 0xFF);
        result.push_back(MAGIC_NUMBER & 0xFF);
        
        // Message type (1 byte)
        result.push_back(static_cast<uint8_t>(type));
        
        // Payload length (4 bytes)
        uint32_t len = static_cast<uint32_t>(payload.size());
        result.push_back((len >> 24) & 0xFF);
        result.push_back((len >> 16) & 0xFF);
        result.push_back((len >> 8) & 0xFF);
        result.push_back(len & 0xFF);
        
        // Timestamp (8 bytes)
        for (int i = 7; i >= 0; --i) {
            result.push_back((timestamp >> (i * 8)) & 0xFF);
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
        uint32_t magic = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        if (magic != MAGIC_NUMBER) {
            throw std::runtime_error("Invalid message: bad magic number");
        }
        
        // Message type
        msg.type = static_cast<MessageType>(data[4]);
        
        // Payload length
        uint32_t len = (data[5] << 24) | (data[6] << 16) | (data[7] << 8) | data[8];
        if (len > MAX_PAYLOAD_SIZE) {
            throw std::runtime_error("Invalid message: payload too large");
        }
        
        // Timestamp
        msg.timestamp = 0;
        for (int i = 0; i < 8; ++i) {
            msg.timestamp = (msg.timestamp << 8) | data[9 + i];
        }
        
        // Payload
        if (data.size() < HEADER_SIZE + len) {
            throw std::runtime_error("Invalid message: incomplete payload");
        }
        msg.payload = std::string(data.begin() + HEADER_SIZE, data.begin() + HEADER_SIZE + len);
        
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
                << "," << tx->getSignature();
        }
        
        return oss.str();
    }
    
    static std::shared_ptr<Block> deserialize(const std::string& data) {
        std::istringstream iss(data);
        std::string token;
        
        int index;
        std::string prev_hash, hash, merkle_root;
        int difficulty;
        size_t tx_count;

        std::getline(iss, token, '|'); index = std::stoi(token);
        std::getline(iss, token, '|'); // timestamp — not reconstructed
        std::getline(iss, prev_hash, '|');
        std::getline(iss, hash, '|');
        std::getline(iss, merkle_root, '|');
        std::getline(iss, token, '|'); difficulty = std::stoi(token);
        std::getline(iss, token, '|'); // nonce — not reconstructed
        std::getline(iss, token, '|'); tx_count = std::stoull(token);

        auto block = std::make_shared<Block>(index, prev_hash, difficulty);

        for (size_t i = 0; i < tx_count; ++i) {
            std::string tx_data;
            std::getline(iss, tx_data, '|');

            std::istringstream tx_stream(tx_data);
            std::string sender, receiver, sig;
            double amount;

            std::getline(tx_stream, sender, ',');
            std::getline(tx_stream, receiver, ',');
            std::getline(tx_stream, token, ','); amount = std::stod(token);
            std::getline(tx_stream, token, ','); // tx_timestamp — not reconstructed
            std::getline(tx_stream, sig, ',');

            auto tx = std::make_shared<Transaction>(sender, receiver, amount);
            block->addTransaction(tx);
        }
        
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
            << tx.getSignature();
        return oss.str();
    }
    
    static std::shared_ptr<Transaction> deserialize(const std::string& data) {
        std::istringstream iss(data);
        std::string sender, receiver, sig, token;
        double amount;

        std::getline(iss, sender, '|');
        std::getline(iss, receiver, '|');
        std::getline(iss, token, '|'); amount = std::stod(token);
        std::getline(iss, token, '|'); // timestamp — not reconstructed
        std::getline(iss, sig, '|');

        auto tx = std::make_shared<Transaction>(sender, receiver, amount);
        return tx;
    }
};

}
