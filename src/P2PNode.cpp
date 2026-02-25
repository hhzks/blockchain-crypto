#include "include/P2PNode.h"
#include <sstream>
#include <random>
#include <algorithm>
#include <format>
#include <print>

namespace p2p {

bool Peer::send(const Message& msg) {
    std::scoped_lock lock(send_mutex);
    
    if (socket == INVALID_SOCK) {
        return false;
    }
    
    auto data = msg.serialize();
    size_t total_sent = 0;
    
    while (total_sent < data.size()) {
        int sent = ::send(socket, reinterpret_cast<const char*>(data.data() + total_sent),
                          static_cast<int>(data.size() - total_sent), 0);
        if (sent == SOCKET_ERROR_CODE) {
            return false;
        }
        total_sent += sent;
    }
    
    return true;
}

bool Peer::receive(Message& msg) {
    if (socket == INVALID_SOCK) {
        return false;
    }
    
    std::vector<uint8_t> header(Message::HEADER_SIZE);
    size_t total_received = 0;
    
    while (total_received < Message::HEADER_SIZE) {
        int received = recv(socket, reinterpret_cast<char*>(header.data() + total_received),
                            static_cast<int>(Message::HEADER_SIZE - total_received), 0);
        if (received <= 0) {
            return false;
        }
        total_received += received;
    }
    
    // Verify magic number
    uint32_t magic = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
    if (magic != Message::MAGIC_NUMBER) {
        return false;
    }
    
    uint32_t payload_len = (header[5] << 24) | (header[6] << 16) | (header[7] << 8) | header[8];
    if (payload_len > Message::MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    std::vector<uint8_t> full_data = header;
    full_data.resize(Message::HEADER_SIZE + payload_len);
    total_received = 0;
    
    while (total_received < payload_len) {
        int received = recv(socket, reinterpret_cast<char*>(full_data.data() + Message::HEADER_SIZE + total_received),
                            static_cast<int>(payload_len - total_received), 0);
        if (received <= 0) {
            return false;
        }
        total_received += received;
    }
    
    try {
        msg = Message::deserialize(full_data);
        updateLastSeen();
        return true;
    } catch (...) {
        return false;
    }
}

P2PNode::P2PNode(Blockchain* chain, const P2PConfig& cfg)
    : blockchain(chain), config(cfg), listen_socket(INVALID_SOCK),
      running(false), syncing(false) {
    node_id = generateNodeId();
}

P2PNode::~P2PNode() {
    stop();
}

std::string P2PNode::generateNodeId() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return std::format("{:016x}{:016x}", dis(gen), dis(gen));
}

bool P2PNode::initializeNetwork() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        log("WSAStartup failed: " + std::to_string(result));
        return false;
    }
#endif
    return true;
}

void P2PNode::cleanupNetwork() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool P2PNode::createListenSocket() {
    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCK) {
        log("Failed to create listen socket");
        return false;
    }
    
    int optval = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&optval), sizeof(optval));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config.listen_port);
    
    if (bind(listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_CODE) {
        log("Failed to bind to port " + std::to_string(config.listen_port));
        closesocket(listen_socket);
        listen_socket = INVALID_SOCK;
        return false;
    }
    
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR_CODE) {
        log("Failed to listen on socket");
        closesocket(listen_socket);
        listen_socket = INVALID_SOCK;
        return false;
    }
    
    log("Listening on port " + std::to_string(config.listen_port));
    return true;
}

bool P2PNode::start() {
    if (running) {
        return true;
    }
    
    if (!initializeNetwork()) {
        return false;
    }
    
    if (!createListenSocket()) {
        cleanupNetwork();
        return false;
    }
    
    running = true;
    
    listener_thread = std::jthread(&P2PNode::acceptConnections, this);
    receiver_thread = std::jthread(&P2PNode::receiveMessages, this);
    ping_thread = std::jthread(&P2PNode::pingPeers, this);
    sync_thread = std::jthread(&P2PNode::syncPeriodically, this);
    
    connectToSeedNodes();
    
    log("P2P node started with ID: " + node_id);
    return true;
}

void P2PNode::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    stop_condition.notify_all();
    
    if (listen_socket != INVALID_SOCK) {
        closesocket(listen_socket);
        listen_socket = INVALID_SOCK;
    }
    
    {
        std::scoped_lock lock(peers_mutex);
        for (auto& [addr, peer] : peers) {
            auto msg = Message::createDisconnect("Node shutting down");
            peer->send(msg);
        }
        peers.clear();
    }
    
    if (listener_thread.joinable()) listener_thread.join();
    if (receiver_thread.joinable()) receiver_thread.join();
    if (ping_thread.joinable()) ping_thread.join();
    if (sync_thread.joinable()) sync_thread.join();
    
    cleanupNetwork();
    log("P2P node stopped");
}

bool P2PNode::connectToPeer(const std::string& ip, uint16_t port) {
    if (!running) {
        return false;
    }
    
    std::string address = ip + ":" + std::to_string(port);
    
    {
        std::scoped_lock lock(peers_mutex);
        if (peers.contains(address)) {
            log("Already connected to " + address);
            return true;
        }
        
        if (peers.size() >= config.max_peers) {
            log("Max peers reached, cannot connect to " + address);
            return false;
        }
    }
    
    log("Connecting to " + address + "...");
    
    SocketType sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCK) {
        log("Failed to create socket for " + address);
        return false;
    }
    
    #ifdef _WIN32
        DWORD timeout = 10000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    #else
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    #endif
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        log("Invalid IP address: " + ip);
        closesocket(sock);
        return false;
    }
    
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_CODE) {
        log("Failed to connect to " + address);
        closesocket(sock);
        return false;
    }
    
    auto peer = std::make_shared<Peer>(sock, ip, port);
    peer->setState(PeerState::HANDSHAKING);
    
    auto handshake = Message::createHandshake(node_id, config.listen_port, 
                                               static_cast<int64_t>(blockchain->getChainSize()));
    if (!peer->send(handshake)) {
        log("Failed to send handshake to " + address);
        return false;
    }
    
    {
        std::scoped_lock lock(peers_mutex);
        peers[address] = peer;
    }
    
    log("Connected to " + address);
    return true;
}

void P2PNode::disconnectPeer(const std::string& address) {
    std::shared_ptr<Peer> peer;
    
    {
        std::scoped_lock lock(peers_mutex);
        auto it = peers.find(address);
        if (it == peers.end()) {
            return;
        }
        peer = it->second;
        peers.erase(it);
    }
    
    auto msg = Message::createDisconnect("Disconnected by local node");
    peer->send(msg);
    
    log("Disconnected from " + address);
    
    if (callbacks.onPeerDisconnected) {
        callbacks.onPeerDisconnected(peer);
    }
}

void P2PNode::removePeer(const std::string& address) {
    std::shared_ptr<Peer> peer;
    
    {
        std::scoped_lock lock(peers_mutex);
        auto it = peers.find(address);
        if (it == peers.end()) {
            return;
        }
        peer = it->second;
        peers.erase(it);
    }
    
    log("Peer removed: " + address);
    
    if (callbacks.onPeerDisconnected) {
        callbacks.onPeerDisconnected(peer);
    }
}

void P2PNode::broadcastBlock(std::shared_ptr<Block> block) {
    std::string block_hash = block->getHash();
    
    {
        std::scoped_lock lock(known_mutex);
        if (known_blocks.contains(block_hash)) {
            return;
        }
        known_blocks.insert(block_hash);
        
        if (known_blocks.size() > 10000) {
            auto it = known_blocks.begin();
            std::advance(it, 5000);
            known_blocks.erase(known_blocks.begin(), it);
        }
    }
    
    std::string serialized = BlockSerializer::serialize(*block);
    auto msg = Message::createNewBlock(serialized);
    broadcast(msg);
    
    log("Broadcasted block " + std::to_string(block->getIndex()));
}

void P2PNode::broadcastTransaction(std::shared_ptr<Transaction> tx) {
    std::string tx_hash = tx->calculateHash();
    
    {
        std::scoped_lock lock(known_mutex);
        if (known_txs.contains(tx_hash)) {
            return;
        }
        known_txs.insert(tx_hash);
        
        if (known_txs.size() > 50000) {
            auto it = known_txs.begin();
            std::advance(it, 25000);
            known_txs.erase(known_txs.begin(), it);
        }
    }
    
    std::string serialized = TransactionSerializer::serialize(*tx);
    auto msg = Message::createNewTransaction(serialized);
    broadcast(msg);
    
    log("Broadcasted transaction from " + tx->getSender());
}

void P2PNode::requestSync() {
    if (syncing) {
        return;
    }
    
    auto peer = findBestPeerForSync();
    if (peer) {
        syncWithPeer(peer);
    }
}

size_t P2PNode::getPeerCount() const {
    std::scoped_lock lock(peers_mutex);
    size_t count = 0;
    for (const auto& [addr, peer] : peers) {
        if (peer->getState() == PeerState::CONNECTED) {
            count++;
        }
    }
    return count;
}

std::vector<PeerInfo> P2PNode::getConnectedPeers() const {
    std::vector<PeerInfo> result;
    std::scoped_lock lock(peers_mutex);
    
    for (const auto& [addr, peer] : peers) {
        if (peer->getState() == PeerState::CONNECTED) {
            result.push_back(peer->toPeerInfo());
        }
    }
    
    return result;
}

void P2PNode::acceptConnections() {
    while (running) {
        sockaddr_in client_addr{};
        #ifdef _WIN32
            int addr_len = sizeof(client_addr);
        #else
            socklen_t addr_len = sizeof(client_addr);
        #endif
        
        SocketType client_sock = accept(listen_socket, 
                                       reinterpret_cast<sockaddr*>(&client_addr), 
                                       &addr_len);
        
        if (!running) break;
        
        if (client_sock == INVALID_SOCK) {
            continue;
        }
        
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        uint16_t port = ntohs(client_addr.sin_port);
        std::string address = std::string(ip_str) + ":" + std::to_string(port);
        
        {
            std::scoped_lock lock(peers_mutex);
            if (peers.size() >= config.max_peers) {
                log("Rejected connection from " + address + " (max peers reached)");
                closesocket(client_sock);
                continue;
            }
        }
        
        #ifdef _WIN32
            DWORD timeout = 30000;
            setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
        #else
            struct timeval tv;
            tv.tv_sec = 30;
            tv.tv_usec = 0;
            setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        #endif
        
        auto peer = std::make_shared<Peer>(client_sock, ip_str, port);
        peer->setState(PeerState::HANDSHAKING);
        
        {
            std::scoped_lock lock(peers_mutex);
            peers[address] = peer;
        }
        
        log("Accepted connection from " + address);
    }
}

void P2PNode::receiveMessages() {
    while (running) {
        std::vector<std::pair<std::string, std::shared_ptr<Peer>>> current_peers;
        
        {
            std::scoped_lock lock(peers_mutex);
            for (const auto& [addr, peer] : peers) {
                current_peers.push_back({addr, peer});
            }
        }
        
        for (auto& [addr, peer] : current_peers) {
            if (!running) break;
            
            // Use select() for non-blocking check
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(peer->getSocket(), &readSet);
            
            timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000; // 100ms
            
            int result = select(static_cast<int>(peer->getSocket() + 1), &readSet, nullptr, nullptr, &tv);
            
            if (result > 0 && FD_ISSET(peer->getSocket(), &readSet)) {
                Message msg;
                if (peer->receive(msg)) {
                    handleMessage(peer, msg);
                } else {
                    // Connection lost
                    removePeer(addr);
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void P2PNode::pingPeers() {
    while (running) {
        {
            std::unique_lock<std::mutex> lock(stop_mutex);
            stop_condition.wait_for(lock, std::chrono::milliseconds(config.ping_interval),
                                   [this] { return !running.load(); });
        }
        
        if (!running) break;
        
        std::vector<std::pair<std::string, std::shared_ptr<Peer>>> current_peers;
        
        {
            std::scoped_lock lock(peers_mutex);
            for (const auto& [addr, peer] : peers) {
                if (peer->getState() == PeerState::CONNECTED) {
                    current_peers.push_back({addr, peer});
                }
            }
        }
        
        for (auto& [addr, peer] : current_peers) {
            auto ping = Message::createPing(node_id);
            if (!peer->send(ping)) {
                removePeer(addr);
            }
        }
        
        checkPeerTimeouts();
    }
}

void P2PNode::syncPeriodically() {
    while (running) {
        {
            std::unique_lock<std::mutex> lock(stop_mutex);
            stop_condition.wait_for(lock, std::chrono::milliseconds(config.sync_interval),
                                   [this] { return !running.load(); });
        }
        
        if (!running) break;
        
        if (getPeerCount() < config.min_peers) {
            auto msg = Message::createGetPeers();
            broadcast(msg);
        }
        
        requestSync();
    }
}

void P2PNode::handleMessage(std::shared_ptr<Peer> peer, const Message& msg) {
    if (config.enable_logging) {
        log("Received " + Message::typeToString(msg.getType()) + " from " + peer->getAddress());
    }
    
    switch (msg.getType()) {
        case MessageType::HANDSHAKE:
            handleHandshake(peer, msg);
            break;
        case MessageType::HANDSHAKE_ACK:
            handleHandshake(peer, msg); // Same handler
            break;
        case MessageType::PING:
            handlePing(peer, msg);
            break;
        case MessageType::PONG:
            handlePong(peer, msg);
            break;
        case MessageType::GET_PEERS:
            handleGetPeers(peer, msg);
            break;
        case MessageType::PEERS:
            handlePeers(peer, msg);
            break;
        case MessageType::GET_BLOCKS:
            handleGetBlocks(peer, msg);
            break;
        case MessageType::BLOCKS:
            handleBlocks(peer, msg);
            break;
        case MessageType::GET_BLOCK_HEIGHT:
            handleGetBlockHeight(peer, msg);
            break;
        case MessageType::BLOCK_HEIGHT:
            handleBlockHeight(peer, msg);
            break;
        case MessageType::NEW_BLOCK:
            handleNewBlock(peer, msg);
            break;
        case MessageType::NEW_TRANSACTION:
            handleNewTransaction(peer, msg);
            break;
        case MessageType::DISCONNECT:
            handleDisconnect(peer, msg);
            break;
        default:
            log("Unknown message type from " + peer->getAddress());
    }
}

void P2PNode::handleHandshake(std::shared_ptr<Peer> peer, const Message& msg) {
    std::istringstream iss(msg.getPayload());
    std::string peer_node_id, version;
    uint16_t peer_port;
    int64_t peer_height;
    
    std::getline(iss, peer_node_id, '|');
    std::string token;
    std::getline(iss, token, '|');
    peer_port = static_cast<uint16_t>(std::stoi(token));
    std::getline(iss, token, '|');
    peer_height = std::stoll(token);
    std::getline(iss, version, '|');
    
    if (peer_node_id == node_id) {
        log("Rejecting self-connection");
        removePeer(peer->getAddress());
        return;
    }
    
    peer->setNodeId(peer_node_id);
    peer->setBlockHeight(peer_height);
    peer->setVersion(version);
    peer->setState(PeerState::CONNECTED);
    
    if (msg.getType() == MessageType::HANDSHAKE) {
        auto ack = Message::createHandshake(node_id, config.listen_port,
                                            static_cast<int64_t>(blockchain->getChainSize()));
        ack.setType(MessageType::HANDSHAKE_ACK);
        peer->send(ack);
    }
    
    log("Handshake completed with " + peer->getAddress() + " (node_id: " + peer_node_id + 
        ", height: " + std::to_string(peer_height) + ")");
    
    if (callbacks.onPeerConnected) {
        callbacks.onPeerConnected(peer);
    }
    
    if (peer_height > static_cast<int64_t>(blockchain->getChainSize())) {
        syncWithPeer(peer);
    }
}

void P2PNode::handlePing(std::shared_ptr<Peer> peer, const Message& /*msg*/) {
    auto pong = Message::createPong(node_id);
    peer->send(pong);
}

void P2PNode::handlePong(std::shared_ptr<Peer> peer, const Message& /*msg*/) {
    peer->updateLastSeen();
}

void P2PNode::handleGetPeers(std::shared_ptr<Peer> peer, const Message& /*msg*/) {
    std::vector<PeerInfo> peer_list;
    
    {
        std::scoped_lock lock(peers_mutex);
        for (const auto& [addr, p] : peers) {
            if (p->getState() == PeerState::CONNECTED && p.get() != peer.get()) {
                peer_list.push_back(p->toPeerInfo());
            }
        }
    }
    
    auto response = Message::createPeers(peer_list);
    peer->send(response);
}

void P2PNode::handlePeers(std::shared_ptr<Peer> /*peer*/, const Message& msg) {
    if (msg.getPayload().empty()) {
        return;
    }
    
    std::istringstream iss(msg.getPayload());
    std::string line;
    
    while (std::getline(iss, line, '\n')) {
        if (line.empty()) continue;
        
        try {
            PeerInfo info = PeerInfo::deserialize(line);
            
            if (info.node_id == node_id) {
                continue;
            }
            
            std::string address = info.ip + ":" + std::to_string(info.port);
            bool already_connected = false;
            
            {
                std::scoped_lock lock(peers_mutex);
                already_connected = peers.contains(address);
            }
            
            if (!already_connected && getPeerCount() < config.max_peers) {
                connectToPeer(info.ip, info.port);
            }
        } catch (...) {
        }
    }
}

void P2PNode::handleGetBlocks(std::shared_ptr<Peer> peer, const Message& msg) {
    std::istringstream iss(msg.getPayload());
    std::string token;
    std::getline(iss, token, '|');
    int64_t start_height = std::stoll(token);
    std::getline(iss, token, '|');
    int64_t end_height = std::stoll(token);
    
    const auto& chain = blockchain->getChain();
    
    if (start_height < 0 || start_height >= static_cast<int64_t>(chain.size())) {
        auto error = Message::createError("Invalid block range");
        peer->send(error);
        return;
    }
    
    if (end_height < 0 || end_height >= static_cast<int64_t>(chain.size())) {
        end_height = static_cast<int64_t>(chain.size()) - 1;
    }
    
    std::ostringstream oss;
    oss << (end_height - start_height + 1);
    
    for (int64_t i = start_height; i <= end_height; ++i) {
        oss << "\n" << BlockSerializer::serialize(*chain[i]);
    }
    
    Message response(MessageType::BLOCKS, oss.str());
    peer->send(response);
}

void P2PNode::handleBlocks(std::shared_ptr<Peer> /*peer*/, const Message& msg) {
    std::istringstream iss(msg.getPayload());
    std::string line;
    
    std::getline(iss, line, '\n');
    int block_count = std::stoi(line);
    
    log("Received " + std::to_string(block_count) + " blocks");
    
    int processed = 0;
    while (std::getline(iss, line, '\n')) {
        if (line.empty()) continue;
        
        try {
            auto block = BlockSerializer::deserialize(line);
            
            {
                std::scoped_lock lock(known_mutex);
                known_blocks.insert(block->getHash());
            }
            
            if (callbacks.onNewBlock) {
                callbacks.onNewBlock(block);
            }
            
            processed++;
            
            if (callbacks.onSyncProgress) {
                callbacks.onSyncProgress(processed * 100 / block_count);
            }
        } catch (const std::exception& e) {
            log("Failed to deserialize block: " + std::string(e.what()));
        }
    }
    
    syncing = false;
}

void P2PNode::handleGetBlockHeight(std::shared_ptr<Peer> peer, const Message& /*msg*/) {
    auto response = Message::createBlockHeight(static_cast<int64_t>(blockchain->getChainSize()));
    peer->send(response);
}

void P2PNode::handleBlockHeight(std::shared_ptr<Peer> peer, const Message& msg) {
    int64_t height = std::stoll(msg.getPayload());
    peer->setBlockHeight(height);
    
    if (height > static_cast<int64_t>(blockchain->getChainSize())) {
        syncWithPeer(peer);
    }
}

void P2PNode::handleNewBlock(std::shared_ptr<Peer> peer, const Message& msg) {
    try {
        auto block = BlockSerializer::deserialize(msg.getPayload());
        std::string block_hash = block->getHash();
        
        {
            std::scoped_lock lock(known_mutex);
            if (known_blocks.contains(block_hash)) {
                return;
            }
            known_blocks.insert(block_hash);
        }
        
        log("Received new block " + std::to_string(block->getIndex()) + 
            " from " + peer->getAddress());
        
        if (callbacks.onNewBlock) {
            callbacks.onNewBlock(block);
        }
        
        auto fwd_msg = Message::createNewBlock(msg.getPayload());
        broadcast(fwd_msg, peer->getAddress());
        
    } catch (const std::exception& e) {
        log("Failed to process new block: " + std::string(e.what()));
    }
}

void P2PNode::handleNewTransaction(std::shared_ptr<Peer> peer, const Message& msg) {
    try {
        auto tx = TransactionSerializer::deserialize(msg.getPayload());
        std::string tx_hash = tx->calculateHash();
        
        {
            std::scoped_lock lock(known_mutex);
            if (known_txs.contains(tx_hash)) {
                return;
            }
            known_txs.insert(tx_hash);
        }
        
        log("Received new transaction from " + peer->getAddress());
        
        if (callbacks.onNewTransaction) {
            callbacks.onNewTransaction(tx);
        }
        
        auto fwd_msg = Message::createNewTransaction(msg.getPayload());
        broadcast(fwd_msg, peer->getAddress());
        
    } catch (const std::exception& e) {
        log("Failed to process new transaction: " + std::string(e.what()));
    }
}

void P2PNode::handleDisconnect(std::shared_ptr<Peer> peer, const Message& msg) {
    log("Peer " + peer->getAddress() + " disconnecting: " + msg.getPayload());
    removePeer(peer->getAddress());
}

bool P2PNode::sendToPeer(std::shared_ptr<Peer> peer, const Message& msg) {
    return peer->send(msg);
}

void P2PNode::broadcast(const Message& msg, const std::string& exclude_peer) {
    std::vector<std::shared_ptr<Peer>> peer_list;
    
    {
        std::scoped_lock lock(peers_mutex);
        for (const auto& [addr, peer] : peers) {
            if (peer->getState() == PeerState::CONNECTED && addr != exclude_peer) {
                peer_list.push_back(peer);
            }
        }
    }
    
    for (auto& peer : peer_list) {
        peer->send(msg);
    }
}

void P2PNode::checkPeerTimeouts() {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    std::vector<std::string> timed_out;
    
    {
        std::scoped_lock lock(peers_mutex);
        for (const auto& [addr, peer] : peers) {
            if (peer->getState() == PeerState::CONNECTED &&
                now - peer->getLastSeen() > config.peer_timeout) {
                timed_out.push_back(addr);
            }
        }
    }
    
    for (const auto& addr : timed_out) {
        log("Peer " + addr + " timed out");
        removePeer(addr);
    }
}

void P2PNode::connectToSeedNodes() {
    for (const auto& seed : config.seed_nodes) {
        size_t pos = seed.find(':');
        if (pos == std::string::npos) continue;
        
        std::string ip = seed.substr(0, pos);
        uint16_t port = static_cast<uint16_t>(std::stoi(seed.substr(pos + 1)));
        
        connectToPeer(ip, port);
    }
}

std::shared_ptr<Peer> P2PNode::findBestPeerForSync() {
    std::shared_ptr<Peer> best_peer;
    int64_t max_height = static_cast<int64_t>(blockchain->getChainSize());
    
    std::scoped_lock lock(peers_mutex);
    for (const auto& [addr, peer] : peers) {
        if (peer->getState() == PeerState::CONNECTED &&
            peer->getBlockHeight() > max_height) {
            max_height = peer->getBlockHeight();
            best_peer = peer;
        }
    }
    
    return best_peer;
}

void P2PNode::syncWithPeer(std::shared_ptr<Peer> peer) {
    if (syncing) {
        return;
    }
    
    syncing = true;
    int64_t our_height = static_cast<int64_t>(blockchain->getChainSize());
    int64_t their_height = peer->getBlockHeight();
    
    log("Starting sync with " + peer->getAddress() + 
        " (our height: " + std::to_string(our_height) + 
        ", their height: " + std::to_string(their_height) + ")");
    
    auto request = Message::createGetBlocks(our_height, their_height - 1);
    peer->send(request);
}

void P2PNode::log(const std::string& message) {
    if (!config.enable_logging) return;

    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const auto log_msg = std::format("[P2P {:%H:%M:%S}] {}", now, message);

    if (callbacks.onLog) {
        callbacks.onLog(log_msg);
    } else {
        std::println("{}", log_msg);
    }
}

}
