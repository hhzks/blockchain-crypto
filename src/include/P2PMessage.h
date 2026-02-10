#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <memory>
#include "Block.h"
#include "Transaction.h"

namespace p2p {

/**
 * Message types for P2P protocol
 */
enum class MessageType : uint8_t {
    // Handshake messages
    HANDSHAKE = 0x00,
    HANDSHAKE_ACK = 0x01,
    
    // Peer discovery
    PING = 0x10,
    PONG = 0x11,
    GET_PEERS = 0x12,
    PEERS = 0x13,
    
    // Blockchain sync
    GET_BLOCKS = 0x20,
    BLOCKS = 0x21,
    GET_BLOCK_HEIGHT = 0x22,
    BLOCK_HEIGHT = 0x23,
    
    // Propagation
    NEW_BLOCK = 0x30,
    NEW_TRANSACTION = 0x31,
    
    // Status
    ERROR_MSG = 0xFE,
    DISCONNECT = 0xFF
};

/**
 * Peer information structure
 */
struct PeerInfo {
    std::string ip;
    uint16_t port;
    std::string nodeId;
    int64_t lastSeen;
    
    std::string serialize() const {
        return ip + ":" + std::to_string(port) + ":" + nodeId + ":" + std::to_string(lastSeen);
    }
    
    static PeerInfo deserialize(const std::string& data) {
        PeerInfo info;
        std::istringstream iss(data);
        std::string token;
        
        std::getline(iss, info.ip, ':');
        std::getline(iss, token, ':');
        info.port = static_cast<uint16_t>(std::stoi(token));
        std::getline(iss, info.nodeId, ':');
        std::getline(iss, token, ':');
        info.lastSeen = std::stoll(token);
        
        return info;
    }
};

/**
 * P2P Network Message
 */
class Message {
private:
    MessageType type;
    std::string payload;
    std::string senderId;
    int64_t timestamp;
    
public:
    static constexpr uint32_t MAGIC_NUMBER = 0x424C4B43; // "BLKC"
    static constexpr size_t HEADER_SIZE = 17; // magic(4) + type(1) + length(4) + timestamp(8)
    static constexpr size_t MAX_PAYLOAD_SIZE = 10 * 1024 * 1024; // 10MB max
    
    Message() : type(MessageType::PING), timestamp(0) {}
    
    Message(MessageType t, const std::string& data = "", const std::string& sender = "")
        : type(t), payload(data), senderId(sender) {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    MessageType getType() const { return type; }
    const std::string& getPayload() const { return payload; }
    const std::string& getSenderId() const { return senderId; }
    int64_t getTimestamp() const { return timestamp; }
    
    void setType(MessageType t) { type = t; }
    void setPayload(const std::string& data) { payload = data; }
    void setSenderId(const std::string& id) { senderId = id; }
    
    /**
     * Serialize message to bytes for network transmission
     */
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
    
    /**
     * Deserialize message from bytes
     */
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
    
    /**
     * Create handshake message
     */
    static Message createHandshake(const std::string& nodeId, uint16_t port, int64_t blockHeight) {
        std::ostringstream oss;
        oss << nodeId << "|" << port << "|" << blockHeight << "|1.0.0";
        return Message(MessageType::HANDSHAKE, oss.str());
    }
    
    /**
     * Create ping message
     */
    static Message createPing(const std::string& nodeId) {
        return Message(MessageType::PING, nodeId);
    }
    
    /**
     * Create pong message
     */
    static Message createPong(const std::string& nodeId) {
        return Message(MessageType::PONG, nodeId);
    }
    
    /**
     * Create get_peers request
     */
    static Message createGetPeers() {
        return Message(MessageType::GET_PEERS, "");
    }
    
    /**
     * Create peers response
     */
    static Message createPeers(const std::vector<PeerInfo>& peers) {
        std::ostringstream oss;
        for (size_t i = 0; i < peers.size(); ++i) {
            if (i > 0) oss << "\n";
            oss << peers[i].serialize();
        }
        return Message(MessageType::PEERS, oss.str());
    }
    
    /**
     * Create get_blocks request (from startHeight to endHeight)
     */
    static Message createGetBlocks(int64_t startHeight, int64_t endHeight = -1) {
        std::ostringstream oss;
        oss << startHeight << "|" << endHeight;
        return Message(MessageType::GET_BLOCKS, oss.str());
    }
    
    /**
     * Create block height request
     */
    static Message createGetBlockHeight() {
        return Message(MessageType::GET_BLOCK_HEIGHT, "");
    }
    
    /**
     * Create block height response
     */
    static Message createBlockHeight(int64_t height) {
        return Message(MessageType::BLOCK_HEIGHT, std::to_string(height));
    }
    
    /**
     * Create new block broadcast
     */
    static Message createNewBlock(const std::string& serializedBlock) {
        return Message(MessageType::NEW_BLOCK, serializedBlock);
    }
    
    /**
     * Create new transaction broadcast
     */
    static Message createNewTransaction(const std::string& serializedTx) {
        return Message(MessageType::NEW_TRANSACTION, serializedTx);
    }
    
    /**
     * Create error message
     */
    static Message createError(const std::string& errorMsg) {
        return Message(MessageType::ERROR_MSG, errorMsg);
    }
    
    /**
     * Create disconnect message
     */
    static Message createDisconnect(const std::string& reason = "") {
        return Message(MessageType::DISCONNECT, reason);
    }
    
    /**
     * Get message type as string (for logging)
     */
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

/**
 * Block serialization helper for network transmission
 */
class BlockSerializer {
public:
    /**
     * Serialize a block to string format
     */
    static std::string serialize(const Block& block) {
        std::ostringstream oss;
        
        // Block header
        oss << block.getIndex() << "|"
            << block.getTimestamp() << "|"
            << block.getPreviousHash() << "|"
            << block.getHash() << "|"
            << block.getMerkleRoot() << "|"
            << block.getDifficulty() << "|"
            << block.getNonce() << "|";
        
        // Transaction count
        const auto& txs = block.getTransactions();
        oss << txs.size();
        
        // Transactions
        for (const auto& tx : txs) {
            oss << "|" << tx->getSender()
                << "," << tx->getReceiver()
                << "," << tx->getAmount()
                << "," << tx->getTimestamp()
                << "," << tx->getSignature();
        }
        
        return oss.str();
    }
    
    /**
     * Deserialize a block from string format
     */
    static std::shared_ptr<Block> deserialize(const std::string& data) {
        std::istringstream iss(data);
        std::string token;
        
        // Block header fields
        int index;
        long long timestamp;
        std::string prevHash, hash, merkleRoot;
        int difficulty, nonce;
        size_t txCount;
        
        std::getline(iss, token, '|'); index = std::stoi(token);
        std::getline(iss, token, '|'); timestamp = std::stoll(token);
        std::getline(iss, prevHash, '|');
        std::getline(iss, hash, '|');
        std::getline(iss, merkleRoot, '|');
        std::getline(iss, token, '|'); difficulty = std::stoi(token);
        std::getline(iss, token, '|'); nonce = std::stoi(token);
        std::getline(iss, token, '|'); txCount = std::stoull(token);
        
        // Create block
        auto block = std::make_shared<Block>(index, prevHash, difficulty);
        
        // Parse transactions
        for (size_t i = 0; i < txCount; ++i) {
            std::string txData;
            std::getline(iss, txData, '|');
            
            std::istringstream txStream(txData);
            std::string sender, receiver, sig;
            double amount;
            long long txTimestamp;
            
            std::getline(txStream, sender, ',');
            std::getline(txStream, receiver, ',');
            std::getline(txStream, token, ','); amount = std::stod(token);
            std::getline(txStream, token, ','); txTimestamp = std::stoll(token);
            std::getline(txStream, sig, ',');
            
            auto tx = std::make_shared<Transaction>(sender, receiver, amount);
            // Note: signature would need to be set through a setter method
            block->addTransaction(tx);
        }
        
        return block;
    }
};

/**
 * Transaction serialization helper
 */
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
        long long timestamp;
        
        std::getline(iss, sender, '|');
        std::getline(iss, receiver, '|');
        std::getline(iss, token, '|'); amount = std::stod(token);
        std::getline(iss, token, '|'); timestamp = std::stoll(token);
        std::getline(iss, sig, '|');
        
        auto tx = std::make_shared<Transaction>(sender, receiver, amount);
        // Signature would need to be restored
        return tx;
    }
};

} // namespace p2p
