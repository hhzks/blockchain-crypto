#include "include/P2PNode.h"
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iomanip>

namespace p2p {

// ============================================================================
// Peer Implementation
// ============================================================================

bool Peer::send(const Message& msg) {
    std::lock_guard<std::mutex> lock(sendMutex);
    
    if (socket == INVALID_SOCK) {
        return false;
    }
    
    auto data = msg.serialize();
    size_t totalSent = 0;
    
    while (totalSent < data.size()) {
        int sent = ::send(socket, reinterpret_cast<const char*>(data.data() + totalSent),
                          static_cast<int>(data.size() - totalSent), 0);
        if (sent == SOCKET_ERROR_CODE) {
            return false;
        }
        totalSent += sent;
    }
    
    return true;
}

bool Peer::receive(Message& msg) {
    if (socket == INVALID_SOCK) {
        return false;
    }
    
    // First, receive the header
    std::vector<uint8_t> header(Message::HEADER_SIZE);
    size_t totalReceived = 0;
    
    while (totalReceived < Message::HEADER_SIZE) {
        int received = recv(socket, reinterpret_cast<char*>(header.data() + totalReceived),
                            static_cast<int>(Message::HEADER_SIZE - totalReceived), 0);
        if (received <= 0) {
            return false;
        }
        totalReceived += received;
    }
    
    // Verify magic number
    uint32_t magic = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
    if (magic != Message::MAGIC_NUMBER) {
        return false;
    }
    
    // Get payload length
    uint32_t payloadLen = (header[5] << 24) | (header[6] << 16) | (header[7] << 8) | header[8];
    if (payloadLen > Message::MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    // Receive payload
    std::vector<uint8_t> fullData = header;
    fullData.resize(Message::HEADER_SIZE + payloadLen);
    totalReceived = 0;
    
    while (totalReceived < payloadLen) {
        int received = recv(socket, reinterpret_cast<char*>(fullData.data() + Message::HEADER_SIZE + totalReceived),
                            static_cast<int>(payloadLen - totalReceived), 0);
        if (received <= 0) {
            return false;
        }
        totalReceived += received;
    }
    
    try {
        msg = Message::deserialize(fullData);
        updateLastSeen();
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// P2PNode Implementation
// ============================================================================

P2PNode::P2PNode(Blockchain* chain, const P2PConfig& cfg)
    : blockchain(chain), config(cfg), listenSocket(INVALID_SOCK),
      running(false), syncing(false) {
    nodeId = generateNodeId();
}

P2PNode::~P2PNode() {
    stop();
}

std::string P2PNode::generateNodeId() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(16) << dis(gen);
    ss << std::setw(16) << dis(gen);
    return ss.str();
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
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCK) {
        log("Failed to create listen socket");
        return false;
    }
    
    // Allow address reuse
    int optval = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&optval), sizeof(optval));
    
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config.listenPort);
    
    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_CODE) {
        log("Failed to bind to port " + std::to_string(config.listenPort));
        closesocket(listenSocket);
        listenSocket = INVALID_SOCK;
        return false;
    }
    
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR_CODE) {
        log("Failed to listen on socket");
        closesocket(listenSocket);
        listenSocket = INVALID_SOCK;
        return false;
    }
    
    log("Listening on port " + std::to_string(config.listenPort));
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
    
    // Start threads
    listenerThread = std::thread(&P2PNode::acceptConnections, this);
    receiverThread = std::thread(&P2PNode::receiveMessages, this);
    pingThread = std::thread(&P2PNode::pingPeers, this);
    syncThread = std::thread(&P2PNode::syncPeriodically, this);
    
    // Connect to seed nodes
    connectToSeedNodes();
    
    log("P2P node started with ID: " + nodeId);
    return true;
}

void P2PNode::stop() {
    if (!running) {
        return;
    }
    
    running = false;
    
    // Signal stop condition
    stopCondition.notify_all();
    
    // Close listen socket to unblock accept()
    if (listenSocket != INVALID_SOCK) {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCK;
    }
    
    // Disconnect all peers
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        for (auto& [addr, peer] : peers) {
            auto msg = Message::createDisconnect("Node shutting down");
            peer->send(msg);
        }
        peers.clear();
    }
    
    // Wait for threads
    if (listenerThread.joinable()) listenerThread.join();
    if (receiverThread.joinable()) receiverThread.join();
    if (pingThread.joinable()) pingThread.join();
    if (syncThread.joinable()) syncThread.join();
    
    cleanupNetwork();
    log("P2P node stopped");
}

bool P2PNode::connectToPeer(const std::string& ip, uint16_t port) {
    if (!running) {
        return false;
    }
    
    std::string address = ip + ":" + std::to_string(port);
    
    // Check if already connected
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        if (peers.find(address) != peers.end()) {
            log("Already connected to " + address);
            return true;
        }
        
        if (peers.size() >= config.maxPeers) {
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
    
    // Set socket timeout
    #ifdef _WIN32
        DWORD timeout = 10000; // 10 seconds
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
    
    // Send handshake
    auto handshake = Message::createHandshake(nodeId, config.listenPort, 
                                               static_cast<int64_t>(blockchain->getChainSize()));
    if (!peer->send(handshake)) {
        log("Failed to send handshake to " + address);
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        peers[address] = peer;
    }
    
    log("Connected to " + address);
    return true;
}

void P2PNode::disconnectPeer(const std::string& address) {
    std::shared_ptr<Peer> peer;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex);
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
        std::lock_guard<std::mutex> lock(peersMutex);
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
    std::string blockHash = block->getHash();
    
    {
        std::lock_guard<std::mutex> lock(knownMutex);
        if (knownBlocks.count(blockHash)) {
            return; // Already known
        }
        knownBlocks.insert(blockHash);
        
        // Limit known blocks cache
        if (knownBlocks.size() > 10000) {
            // Clear half of them
            auto it = knownBlocks.begin();
            std::advance(it, 5000);
            knownBlocks.erase(knownBlocks.begin(), it);
        }
    }
    
    std::string serialized = BlockSerializer::serialize(*block);
    auto msg = Message::createNewBlock(serialized);
    broadcast(msg);
    
    log("Broadcasted block " + std::to_string(block->getIndex()));
}

void P2PNode::broadcastTransaction(std::shared_ptr<Transaction> tx) {
    std::string txHash = tx->calculateHash();
    
    {
        std::lock_guard<std::mutex> lock(knownMutex);
        if (knownTxs.count(txHash)) {
            return; // Already known
        }
        knownTxs.insert(txHash);
        
        // Limit known transactions cache
        if (knownTxs.size() > 50000) {
            auto it = knownTxs.begin();
            std::advance(it, 25000);
            knownTxs.erase(knownTxs.begin(), it);
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
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(peersMutex));
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
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(peersMutex));
    
    for (const auto& [addr, peer] : peers) {
        if (peer->getState() == PeerState::CONNECTED) {
            result.push_back(peer->toPeerInfo());
        }
    }
    
    return result;
}

void P2PNode::acceptConnections() {
    while (running) {
        sockaddr_in clientAddr{};
        #ifdef _WIN32
            int addrLen = sizeof(clientAddr);
        #else
            socklen_t addrLen = sizeof(clientAddr);
        #endif
        
        SocketType clientSock = accept(listenSocket, 
                                       reinterpret_cast<sockaddr*>(&clientAddr), 
                                       &addrLen);
        
        if (!running) break;
        
        if (clientSock == INVALID_SOCK) {
            continue;
        }
        
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        uint16_t port = ntohs(clientAddr.sin_port);
        std::string address = std::string(ipStr) + ":" + std::to_string(port);
        
        {
            std::lock_guard<std::mutex> lock(peersMutex);
            if (peers.size() >= config.maxPeers) {
                log("Rejected connection from " + address + " (max peers reached)");
                closesocket(clientSock);
                continue;
            }
        }
        
        // Set socket timeout
        #ifdef _WIN32
            DWORD timeout = 30000;
            setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
        #else
            struct timeval tv;
            tv.tv_sec = 30;
            tv.tv_usec = 0;
            setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(clientSock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        #endif
        
        auto peer = std::make_shared<Peer>(clientSock, ipStr, port);
        peer->setState(PeerState::HANDSHAKING);
        
        {
            std::lock_guard<std::mutex> lock(peersMutex);
            peers[address] = peer;
        }
        
        log("Accepted connection from " + address);
    }
}

void P2PNode::receiveMessages() {
    while (running) {
        std::vector<std::pair<std::string, std::shared_ptr<Peer>>> currentPeers;
        
        {
            std::lock_guard<std::mutex> lock(peersMutex);
            for (const auto& [addr, peer] : peers) {
                currentPeers.push_back({addr, peer});
            }
        }
        
        for (auto& [addr, peer] : currentPeers) {
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
            std::unique_lock<std::mutex> lock(stopMutex);
            stopCondition.wait_for(lock, std::chrono::milliseconds(config.pingInterval),
                                   [this] { return !running.load(); });
        }
        
        if (!running) break;
        
        std::vector<std::pair<std::string, std::shared_ptr<Peer>>> currentPeers;
        
        {
            std::lock_guard<std::mutex> lock(peersMutex);
            for (const auto& [addr, peer] : peers) {
                if (peer->getState() == PeerState::CONNECTED) {
                    currentPeers.push_back({addr, peer});
                }
            }
        }
        
        for (auto& [addr, peer] : currentPeers) {
            auto ping = Message::createPing(nodeId);
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
            std::unique_lock<std::mutex> lock(stopMutex);
            stopCondition.wait_for(lock, std::chrono::milliseconds(config.syncInterval),
                                   [this] { return !running.load(); });
        }
        
        if (!running) break;
        
        // Check if we need more peers
        if (getPeerCount() < config.minPeers) {
            // Request peers from connected nodes
            auto msg = Message::createGetPeers();
            broadcast(msg);
        }
        
        // Request sync if we might be behind
        requestSync();
    }
}

void P2PNode::handleMessage(std::shared_ptr<Peer> peer, const Message& msg) {
    if (config.enableLogging) {
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
    // Parse handshake: nodeId|port|blockHeight|version
    std::istringstream iss(msg.getPayload());
    std::string peerNodeId, version;
    uint16_t peerPort;
    int64_t peerHeight;
    
    std::getline(iss, peerNodeId, '|');
    std::string token;
    std::getline(iss, token, '|');
    peerPort = static_cast<uint16_t>(std::stoi(token));
    std::getline(iss, token, '|');
    peerHeight = std::stoll(token);
    std::getline(iss, version, '|');
    
    // Don't connect to ourselves
    if (peerNodeId == nodeId) {
        log("Rejecting self-connection");
        removePeer(peer->getAddress());
        return;
    }
    
    peer->setNodeId(peerNodeId);
    peer->setBlockHeight(peerHeight);
    peer->setVersion(version);
    peer->setState(PeerState::CONNECTED);
    
    // If this is a handshake (not ack), send our handshake back
    if (msg.getType() == MessageType::HANDSHAKE) {
        auto ack = Message::createHandshake(nodeId, config.listenPort,
                                            static_cast<int64_t>(blockchain->getChainSize()));
        ack.setType(MessageType::HANDSHAKE_ACK);
        peer->send(ack);
    }
    
    log("Handshake completed with " + peer->getAddress() + " (nodeId: " + peerNodeId + 
        ", height: " + std::to_string(peerHeight) + ")");
    
    if (callbacks.onPeerConnected) {
        callbacks.onPeerConnected(peer);
    }
    
    // Check if we need to sync
    if (peerHeight > static_cast<int64_t>(blockchain->getChainSize())) {
        syncWithPeer(peer);
    }
}

void P2PNode::handlePing(std::shared_ptr<Peer> peer, const Message& /*msg*/) {
    auto pong = Message::createPong(nodeId);
    peer->send(pong);
}

void P2PNode::handlePong(std::shared_ptr<Peer> peer, const Message& /*msg*/) {
    peer->updateLastSeen();
}

void P2PNode::handleGetPeers(std::shared_ptr<Peer> peer, const Message& /*msg*/) {
    std::vector<PeerInfo> peerList;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        for (const auto& [addr, p] : peers) {
            if (p->getState() == PeerState::CONNECTED && p.get() != peer.get()) {
                peerList.push_back(p->toPeerInfo());
            }
        }
    }
    
    auto response = Message::createPeers(peerList);
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
            
            // Don't connect to ourselves
            if (info.nodeId == nodeId) {
                continue;
            }
            
            // Check if already connected
            std::string address = info.ip + ":" + std::to_string(info.port);
            bool alreadyConnected = false;
            
            {
                std::lock_guard<std::mutex> lock(peersMutex);
                alreadyConnected = peers.find(address) != peers.end();
            }
            
            if (!alreadyConnected && getPeerCount() < config.maxPeers) {
                connectToPeer(info.ip, info.port);
            }
        } catch (...) {
            // Invalid peer info, skip
        }
    }
}

void P2PNode::handleGetBlocks(std::shared_ptr<Peer> peer, const Message& msg) {
    // Parse request: startHeight|endHeight
    std::istringstream iss(msg.getPayload());
    std::string token;
    std::getline(iss, token, '|');
    int64_t startHeight = std::stoll(token);
    std::getline(iss, token, '|');
    int64_t endHeight = std::stoll(token);
    
    const auto& chain = blockchain->getChain();
    
    if (startHeight < 0 || startHeight >= static_cast<int64_t>(chain.size())) {
        auto error = Message::createError("Invalid block range");
        peer->send(error);
        return;
    }
    
    if (endHeight < 0 || endHeight >= static_cast<int64_t>(chain.size())) {
        endHeight = static_cast<int64_t>(chain.size()) - 1;
    }
    
    // Serialize blocks
    std::ostringstream oss;
    oss << (endHeight - startHeight + 1); // Block count
    
    for (int64_t i = startHeight; i <= endHeight; ++i) {
        oss << "\n" << BlockSerializer::serialize(*chain[i]);
    }
    
    Message response(MessageType::BLOCKS, oss.str());
    peer->send(response);
}

void P2PNode::handleBlocks(std::shared_ptr<Peer> /*peer*/, const Message& msg) {
    std::istringstream iss(msg.getPayload());
    std::string line;
    
    // First line is block count
    std::getline(iss, line, '\n');
    int blockCount = std::stoi(line);
    
    log("Received " + std::to_string(blockCount) + " blocks");
    
    int processed = 0;
    while (std::getline(iss, line, '\n')) {
        if (line.empty()) continue;
        
        try {
            auto block = BlockSerializer::deserialize(line);
            
            // Add block hash to known blocks
            {
                std::lock_guard<std::mutex> lock(knownMutex);
                knownBlocks.insert(block->getHash());
            }
            
            if (callbacks.onNewBlock) {
                callbacks.onNewBlock(block);
            }
            
            processed++;
            
            if (callbacks.onSyncProgress) {
                callbacks.onSyncProgress(processed * 100 / blockCount);
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
        std::string blockHash = block->getHash();
        
        // Check if we've seen this block
        {
            std::lock_guard<std::mutex> lock(knownMutex);
            if (knownBlocks.count(blockHash)) {
                return; // Already processed
            }
            knownBlocks.insert(blockHash);
        }
        
        log("Received new block " + std::to_string(block->getIndex()) + 
            " from " + peer->getAddress());
        
        if (callbacks.onNewBlock) {
            callbacks.onNewBlock(block);
        }
        
        // Re-broadcast to other peers
        auto fwdMsg = Message::createNewBlock(msg.getPayload());
        broadcast(fwdMsg, peer->getAddress());
        
    } catch (const std::exception& e) {
        log("Failed to process new block: " + std::string(e.what()));
    }
}

void P2PNode::handleNewTransaction(std::shared_ptr<Peer> peer, const Message& msg) {
    try {
        auto tx = TransactionSerializer::deserialize(msg.getPayload());
        std::string txHash = tx->calculateHash();
        
        // Check if we've seen this transaction
        {
            std::lock_guard<std::mutex> lock(knownMutex);
            if (knownTxs.count(txHash)) {
                return; // Already processed
            }
            knownTxs.insert(txHash);
        }
        
        log("Received new transaction from " + peer->getAddress());
        
        if (callbacks.onNewTransaction) {
            callbacks.onNewTransaction(tx);
        }
        
        // Re-broadcast to other peers
        auto fwdMsg = Message::createNewTransaction(msg.getPayload());
        broadcast(fwdMsg, peer->getAddress());
        
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

void P2PNode::broadcast(const Message& msg, const std::string& excludePeer) {
    std::vector<std::shared_ptr<Peer>> peerList;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        for (const auto& [addr, peer] : peers) {
            if (peer->getState() == PeerState::CONNECTED && addr != excludePeer) {
                peerList.push_back(peer);
            }
        }
    }
    
    for (auto& peer : peerList) {
        peer->send(msg);
    }
}

void P2PNode::checkPeerTimeouts() {
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    std::vector<std::string> timedOut;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex);
        for (const auto& [addr, peer] : peers) {
            if (peer->getState() == PeerState::CONNECTED &&
                now - peer->getLastSeen() > config.peerTimeout) {
                timedOut.push_back(addr);
            }
        }
    }
    
    for (const auto& addr : timedOut) {
        log("Peer " + addr + " timed out");
        removePeer(addr);
    }
}

void P2PNode::connectToSeedNodes() {
    for (const auto& seed : config.seedNodes) {
        // Parse seed: ip:port
        size_t pos = seed.find(':');
        if (pos == std::string::npos) continue;
        
        std::string ip = seed.substr(0, pos);
        uint16_t port = static_cast<uint16_t>(std::stoi(seed.substr(pos + 1)));
        
        connectToPeer(ip, port);
    }
}

std::shared_ptr<Peer> P2PNode::findBestPeerForSync() {
    std::shared_ptr<Peer> bestPeer;
    int64_t maxHeight = static_cast<int64_t>(blockchain->getChainSize());
    
    std::lock_guard<std::mutex> lock(peersMutex);
    for (const auto& [addr, peer] : peers) {
        if (peer->getState() == PeerState::CONNECTED &&
            peer->getBlockHeight() > maxHeight) {
            maxHeight = peer->getBlockHeight();
            bestPeer = peer;
        }
    }
    
    return bestPeer;
}

void P2PNode::syncWithPeer(std::shared_ptr<Peer> peer) {
    if (syncing) {
        return;
    }
    
    syncing = true;
    int64_t ourHeight = static_cast<int64_t>(blockchain->getChainSize());
    int64_t theirHeight = peer->getBlockHeight();
    
    log("Starting sync with " + peer->getAddress() + 
        " (our height: " + std::to_string(ourHeight) + 
        ", their height: " + std::to_string(theirHeight) + ")");
    
    auto request = Message::createGetBlocks(ourHeight, theirHeight - 1);
    peer->send(request);
}

void P2PNode::log(const std::string& message) {
    if (!config.enableLogging) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << "[P2P " << std::put_time(std::localtime(&time), "%H:%M:%S") << "] " << message;
    
    if (callbacks.onLog) {
        callbacks.onLog(oss.str());
    } else {
        std::cout << oss.str() << std::endl;
    }
}

} // namespace p2p
