#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>
#include <condition_variable>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SocketType = SOCKET;
    #define INVALID_SOCK INVALID_SOCKET
    #define SOCKET_ERROR_CODE SOCKET_ERROR
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    using SocketType = int;
    #define INVALID_SOCK -1
    #define SOCKET_ERROR_CODE -1
    #define closesocket close
#endif

#include "P2PMessage.h"
#include "Blockchain.h"

namespace p2p {

/**
 * Peer connection state
 */
enum class PeerState {
    CONNECTING,
    HANDSHAKING,
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED
};

/**
 * Represents a connected peer
 */
class Peer {
private:
    SocketType socket;
    std::string nodeId;
    std::string ip;
    uint16_t port;
    PeerState state;
    int64_t lastSeen;
    int64_t connectedAt;
    int64_t blockHeight;
    std::string version;
    
    std::mutex sendMutex;
    std::queue<Message> outgoingQueue;
    
public:
    Peer(SocketType sock, const std::string& peerIp, uint16_t peerPort)
        : socket(sock), ip(peerIp), port(peerPort), state(PeerState::CONNECTING),
          lastSeen(0), connectedAt(0), blockHeight(0) {
        connectedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        lastSeen = connectedAt;
    }
    
    ~Peer() {
        if (socket != INVALID_SOCK) {
            closesocket(socket);
        }
    }
    
    SocketType getSocket() const { return socket; }
    const std::string& getNodeId() const { return nodeId; }
    const std::string& getIp() const { return ip; }
    uint16_t getPort() const { return port; }
    PeerState getState() const { return state; }
    int64_t getLastSeen() const { return lastSeen; }
    int64_t getBlockHeight() const { return blockHeight; }
    const std::string& getVersion() const { return version; }
    
    void setNodeId(const std::string& id) { nodeId = id; }
    void setState(PeerState s) { state = s; }
    void setBlockHeight(int64_t h) { blockHeight = h; }
    void setVersion(const std::string& v) { version = v; }
    void updateLastSeen() {
        lastSeen = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    std::string getAddress() const {
        return ip + ":" + std::to_string(port);
    }
    
    PeerInfo toPeerInfo() const {
        return PeerInfo{ip, port, nodeId, lastSeen};
    }
    
    /**
     * Send a message to this peer
     */
    bool send(const Message& msg);
    
    /**
     * Receive a message from this peer
     */
    bool receive(Message& msg);
};

/**
 * Event callbacks for P2P events
 */
struct P2PCallbacks {
    std::function<void(std::shared_ptr<Peer>)> onPeerConnected;
    std::function<void(std::shared_ptr<Peer>)> onPeerDisconnected;
    std::function<void(std::shared_ptr<Block>)> onNewBlock;
    std::function<void(std::shared_ptr<Transaction>)> onNewTransaction;
    std::function<void(const std::string&)> onLog;
    std::function<void(int64_t)> onSyncProgress;
};

/**
 * Configuration for P2P node
 */
struct P2PConfig {
    uint16_t listenPort = 8333;
    size_t maxPeers = 25;
    size_t minPeers = 3;
    int64_t pingInterval = 30000;      // 30 seconds
    int64_t peerTimeout = 90000;       // 90 seconds
    int64_t syncInterval = 60000;      // 60 seconds
    bool enableLogging = true;
    std::vector<std::string> seedNodes;
};

/**
 * P2P Network Node
 * Handles peer connections, message routing, and blockchain synchronization
 */
class P2PNode {
private:
    std::string nodeId;
    P2PConfig config;
    P2PCallbacks callbacks;
    Blockchain* blockchain;
    
    SocketType listenSocket;
    std::atomic<bool> running;
    std::atomic<bool> syncing;
    
    std::unordered_map<std::string, std::shared_ptr<Peer>> peers;
    std::mutex peersMutex;
    
    std::unordered_set<std::string> knownBlocks;      // Recently seen block hashes
    std::unordered_set<std::string> knownTxs;         // Recently seen transaction hashes
    std::mutex knownMutex;
    
    std::thread listenerThread;
    std::thread receiverThread;
    std::thread pingThread;
    std::thread syncThread;
    
    std::condition_variable stopCondition;
    std::mutex stopMutex;
    
public:
    /**
     * Constructor
     * @param chain Reference to blockchain
     * @param cfg Configuration options
     */
    P2PNode(Blockchain* chain, const P2PConfig& cfg = P2PConfig());
    
    /**
     * Destructor - stops the node
     */
    ~P2PNode();
    
    // Non-copyable
    P2PNode(const P2PNode&) = delete;
    P2PNode& operator=(const P2PNode&) = delete;
    
    /**
     * Start the P2P node
     * @return True if started successfully
     */
    bool start();
    
    /**
     * Stop the P2P node
     */
    void stop();
    
    /**
     * Check if node is running
     */
    bool isRunning() const { return running; }
    
    /**
     * Check if currently syncing
     */
    bool isSyncing() const { return syncing; }
    
    /**
     * Connect to a peer
     * @param ip Peer IP address
     * @param port Peer port
     * @return True if connection initiated
     */
    bool connectToPeer(const std::string& ip, uint16_t port);
    
    /**
     * Disconnect from a peer
     * @param address Peer address (ip:port)
     */
    void disconnectPeer(const std::string& address);
    
    /**
     * Broadcast a new block to all peers
     * @param block Block to broadcast
     */
    void broadcastBlock(std::shared_ptr<Block> block);
    
    /**
     * Broadcast a new transaction to all peers
     * @param tx Transaction to broadcast
     */
    void broadcastTransaction(std::shared_ptr<Transaction> tx);
    
    /**
     * Request blockchain sync from peers
     */
    void requestSync();
    
    /**
     * Get number of connected peers
     */
    size_t getPeerCount() const;
    
    /**
     * Get list of connected peers
     */
    std::vector<PeerInfo> getConnectedPeers() const;
    
    /**
     * Get node ID
     */
    const std::string& getNodeId() const { return nodeId; }
    
    /**
     * Get listening port
     */
    uint16_t getPort() const { return config.listenPort; }
    
    /**
     * Set callbacks for P2P events
     */
    void setCallbacks(const P2PCallbacks& cb) { callbacks = cb; }
    
private:
    /**
     * Generate unique node ID
     */
    std::string generateNodeId();
    
    /**
     * Initialize network (platform-specific)
     */
    bool initializeNetwork();
    
    /**
     * Cleanup network resources
     */
    void cleanupNetwork();
    
    /**
     * Create listening socket
     */
    bool createListenSocket();
    
    /**
     * Accept incoming connections (thread function)
     */
    void acceptConnections();
    
    /**
     * Receive messages from peers (thread function)
     */
    void receiveMessages();
    
    /**
     * Ping peers periodically (thread function)
     */
    void pingPeers();
    
    /**
     * Periodic sync check (thread function)
     */
    void syncPeriodically();
    
    /**
     * Handle incoming message from a peer
     */
    void handleMessage(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle handshake message
     */
    void handleHandshake(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle ping message
     */
    void handlePing(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle pong message
     */
    void handlePong(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle get_peers request
     */
    void handleGetPeers(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle peers response
     */
    void handlePeers(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle get_blocks request
     */
    void handleGetBlocks(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle blocks response
     */
    void handleBlocks(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle get_block_height request
     */
    void handleGetBlockHeight(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle block_height response
     */
    void handleBlockHeight(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle new block announcement
     */
    void handleNewBlock(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle new transaction announcement
     */
    void handleNewTransaction(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Handle disconnect message
     */
    void handleDisconnect(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Send message to a specific peer
     */
    bool sendToPeer(std::shared_ptr<Peer> peer, const Message& msg);
    
    /**
     * Send message to all connected peers
     */
    void broadcast(const Message& msg, const std::string& excludePeer = "");
    
    /**
     * Remove disconnected peer
     */
    void removePeer(const std::string& address);
    
    /**
     * Check and remove timed-out peers
     */
    void checkPeerTimeouts();
    
    /**
     * Connect to seed nodes
     */
    void connectToSeedNodes();
    
    /**
     * Find best peer for syncing
     */
    std::shared_ptr<Peer> findBestPeerForSync();
    
    /**
     * Synchronize blockchain with a peer
     */
    void syncWithPeer(std::shared_ptr<Peer> peer);
    
    /**
     * Log message (if logging enabled)
     */
    void log(const std::string& message);
};

} // namespace p2p
