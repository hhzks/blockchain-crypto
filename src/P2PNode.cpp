#include "include/P2PNode.h"
#include "include/utils.h"
#include <cerrno>
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

namespace {

// A frame is the fixed header plus a 16-bit-counted sender id plus the
// payload, so this is the largest a legitimate one can be.
constexpr size_t MAX_FRAME_SIZE =
    Message::HEADER_SIZE + 0xFFFF + Message::MAX_PAYLOAD_SIZE;

// Cap on messages drained from one peer per readable event, so a peer that
// keeps its socket hot cannot starve the others.
constexpr int MAX_MESSAGES_PER_ROUND = 32;

int lastSocketError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

} // namespace

ReceiveStatus Peer::extractMessage(Message& msg) {
    if (recv_buffer.size() < Message::HEADER_SIZE) {
        return ReceiveStatus::Incomplete;
    }

    if (readU32BE(recv_buffer.data()) != Message::MAGIC_NUMBER) {
        return ReceiveStatus::Closed;
    }

    const uint32_t payload_len = readU32BE(recv_buffer.data() + 5);
    if (payload_len > Message::MAX_PAYLOAD_SIZE) {
        return ReceiveStatus::Closed;
    }

    // The sender id is length-prefixed and sits between the fixed header and
    // the payload, so both variable fields size the frame.
    const uint16_t sender_len = readU16BE(recv_buffer.data() + 17);
    const size_t frame_size =
        Message::HEADER_SIZE + static_cast<size_t>(sender_len) + payload_len;

    if (recv_buffer.size() < frame_size) {
        return ReceiveStatus::Incomplete;
    }

    try {
        msg = Message::deserialize(std::vector<uint8_t>(
            recv_buffer.begin(), recv_buffer.begin() + frame_size));
    } catch (...) {
        return ReceiveStatus::Closed;
    }

    recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + frame_size);
    updateLastSeen();
    return ReceiveStatus::Message;
}

ReceiveStatus Peer::nextBufferedMessage(Message& msg) {
    if (socket == INVALID_SOCK) {
        return ReceiveStatus::Closed;
    }
    return extractMessage(msg);
}

ReceiveStatus Peer::pollReceive(Message& msg) {
    if (socket == INVALID_SOCK) {
        return ReceiveStatus::Closed;
    }

    // Exactly one read. The caller only gets here when select() said the
    // socket is readable, so this returns as soon as any bytes are available
    // and never waits for the rest of a partial frame.
    uint8_t chunk[4096];
    const int received = recv(socket, reinterpret_cast<char*>(chunk),
                              static_cast<int>(sizeof chunk), 0);
    if (received <= 0) {
        return ReceiveStatus::Closed;
    }

    recv_buffer.insert(recv_buffer.end(), chunk, chunk + received);
    if (recv_buffer.size() > MAX_FRAME_SIZE) {
        return ReceiveStatus::Closed;
    }

    return extractMessage(msg);
}

P2PNode::P2PNode(Blockchain* chain, const P2PConfig& cfg)
    : config(cfg), blockchain(chain), listen_socket(INVALID_SOCK),
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
    
    // The store has to happen under stop_mutex: a worker that has evaluated
    // the predicate as false but has not yet blocked on the condition variable
    // would otherwise miss the notification and sleep out its whole interval
    // (30s for pings, 60s for sync), which stop() then waits on in join().
    {
        std::scoped_lock lock(stop_mutex);
        running = false;
    }
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
            // A persistent listen-socket error -- descriptor exhaustion is the
            // realistic one, and it is reachable because every accepted
            // connection holds a socket until its peer is removed -- used to
            // spin this loop at 100% of a core. Back off, and stay woken by
            // stop().
            log("accept() failed with error " + std::to_string(lastSocketError()));
            std::unique_lock<std::mutex> lock(stop_mutex);
            stop_condition.wait_for(lock, std::chrono::milliseconds(100),
                                    [this] { return !running.load(); });
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

        // One select over every peer socket, rather than a 100ms select per
        // peer plus a fixed 50ms sleep: message latency no longer grows with
        // the peer count, and shutdown is noticed within one timeout.
        fd_set read_set;
        FD_ZERO(&read_set);
        SocketType max_socket = 0;
        size_t watched = 0;

        for (const auto& [addr, peer] : current_peers) {
            const SocketType sock = peer->getSocket();
            if (sock == INVALID_SOCK) continue;
            if (watched >= FD_SETSIZE) break;
#ifndef _WIN32
            // POSIX fd_set is indexed by descriptor number.
            if (sock >= FD_SETSIZE) continue;
#endif
            FD_SET(sock, &read_set);
            watched++;
            if (sock > max_socket) max_socket = sock;
        }

        if (watched == 0) {
            std::unique_lock<std::mutex> lock(stop_mutex);
            stop_condition.wait_for(lock, std::chrono::milliseconds(100),
                                    [this] { return !running.load(); });
            continue;
        }

        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms

        const int ready = select(static_cast<int>(max_socket + 1), &read_set,
                                 nullptr, nullptr, &tv);
        if (ready <= 0) {
            continue; // timeout, or a peer was removed under us; re-poll
        }

        for (const auto& [addr, peer] : current_peers) {
            if (!running) break;

            const SocketType sock = peer->getSocket();
            if (sock == INVALID_SOCK || !FD_ISSET(sock, &read_set)) continue;

            Message msg;
            ReceiveStatus status = peer->pollReceive(msg);

            for (int drained = 0;
                 status == ReceiveStatus::Message && drained < MAX_MESSAGES_PER_ROUND;
                 drained++) {
                // Backstop: an exception escaping a handler would leave this
                // jthread and terminate the process. One bad peer must cost
                // that peer, not the node.
                try {
                    handleMessage(peer, msg);
                } catch (const std::exception& e) {
                    log("Dropping peer " + addr + " after handler error: " +
                        std::string(e.what()));
                    status = ReceiveStatus::Closed;
                    break;
                }
                // Anything else that arrived in the same read, without
                // touching the socket again.
                status = peer->nextBufferedMessage(msg);
            }

            if (status == ReceiveStatus::Closed) {
                removePeer(addr);
            }
        }
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
    int64_t peer_height = 0;

    std::getline(iss, peer_node_id, '|');
    std::string token;
    std::getline(iss, token, '|'); // peer_port — unused; connection is already established
    std::getline(iss, token, '|');

    // Peer-supplied text: std::stoll threw out of the receiver thread on
    // anything non-numeric, and a short payload leaves the token empty.
    auto parsed_height = utils::parseInt64(token);
    if (!parsed_height || *parsed_height < 0) {
        log("Rejecting handshake from " + peer->getAddress() +
            ": bad block height '" + token + "'");
        removePeer(peer->getAddress());
        return;
    }
    peer_height = *parsed_height;

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
    auto parsed_start = utils::parseInt64(token);
    std::getline(iss, token, '|');
    auto parsed_end = utils::parseInt64(token);

    if (!parsed_start || !parsed_end) {
        peer->send(Message::createError("Malformed block range"));
        return;
    }

    int64_t start_height = *parsed_start;
    int64_t end_height = *parsed_end;
    
    const auto& chain = blockchain->getChain();
    
    if (start_height < 0 || start_height >= static_cast<int64_t>(chain.size())) {
        auto error = Message::createError("Invalid block range");
        peer->send(error);
        return;
    }
    
    if (end_height < 0 || end_height >= static_cast<int64_t>(chain.size())) {
        end_height = static_cast<int64_t>(chain.size()) - 1;
    }

    if (end_height < start_height) {
        peer->send(Message::createError("Invalid block range"));
        return;
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
    auto parsed_count = utils::parseInt(line);
    if (!parsed_count || *parsed_count < 0) {
        log("Ignoring BLOCKS message with bad block count '" + line + "'");
        syncing = false;
        return;
    }
    const int block_count = *parsed_count;
    
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
                // block_count is peer-supplied: 0 used to raise SIGFPE here and
                // a negative value produced nonsense progress.
                callbacks.onSyncProgress(utils::percentComplete(processed, block_count));
            }
        } catch (const std::exception& e) {
            log("Failed to deserialize block: " + std::string(e.what()));
        }
    }

    if (processed != block_count) {
        log("Peer advertised " + std::to_string(block_count) + " blocks but sent "
            + std::to_string(processed));
    }

    syncing = false;
}

void P2PNode::handleGetBlockHeight(std::shared_ptr<Peer> peer, const Message& /*msg*/) {
    auto response = Message::createBlockHeight(static_cast<int64_t>(blockchain->getChainSize()));
    peer->send(response);
}

void P2PNode::handleBlockHeight(std::shared_ptr<Peer> peer, const Message& msg) {
    auto parsed = utils::parseInt64(msg.getPayload());
    if (!parsed || *parsed < 0) {
        log("Ignoring BLOCK_HEIGHT '" + msg.getPayload() + "' from " +
            peer->getAddress());
        return;
    }

    const int64_t height = *parsed;
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
        auto parsed_port = utils::parseInt(seed.substr(pos + 1));
        if (!parsed_port || *parsed_port <= 0 || *parsed_port > 65535) {
            log("Skipping seed node with invalid port: " + seed);
            continue;
        }
        const uint16_t port = static_cast<uint16_t>(*parsed_port);
        
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

uint16_t P2PNode::getActualListenPort() const {
    if (listen_socket == INVALID_SOCK) return 0;
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getsockname(listen_socket, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        return 0;
    }
    return ntohs(addr.sin_port);
}

}
