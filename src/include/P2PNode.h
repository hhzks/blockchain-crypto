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
#include <format>

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

enum class PeerState {
    CONNECTING,
    HANDSHAKING,
    CONNECTED,
    DISCONNECTING,
    DISCONNECTED
};

class Peer {
private:
    SocketType socket;
    std::string node_id;
    std::string ip;
    uint16_t port;
    PeerState state;
    int64_t last_seen;
    int64_t connected_at;
    int64_t block_height;
    std::string version;
    
    std::mutex send_mutex;
    std::queue<Message> outgoing_queue;
    
public:
    Peer(SocketType sock, const std::string& peer_ip, uint16_t peer_port)
        : socket(sock), ip(peer_ip), port(peer_port), state(PeerState::CONNECTING),
          last_seen(0), connected_at(0), block_height(0) {
        connected_at = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        last_seen = connected_at;
    }
    
    ~Peer() {
        if (socket != INVALID_SOCK) {
            closesocket(socket);
        }
    }
    
    SocketType getSocket() const { return socket; }
    const std::string& getNodeId() const { return node_id; }
    const std::string& getIp() const { return ip; }
    uint16_t getPort() const { return port; }
    PeerState getState() const { return state; }
    int64_t getLastSeen() const { return last_seen; }
    int64_t getBlockHeight() const { return block_height; }
    const std::string& getVersion() const { return version; }
    
    void setNodeId(const std::string& id) { node_id = id; }
    void setState(PeerState s) { state = s; }
    void setBlockHeight(int64_t h) { block_height = h; }
    void setVersion(const std::string& v) { version = v; }
    void updateLastSeen() {
        last_seen = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    std::string getAddress() const {
        return ip + ":" + std::to_string(port);
    }
    
    PeerInfo toPeerInfo() const {
        return PeerInfo{ip, port, node_id, last_seen};
    }
    
    bool send(const Message& msg);
    bool receive(Message& msg);
};

struct P2PCallbacks {
    std::function<void(std::shared_ptr<Peer>)> onPeerConnected;
    std::function<void(std::shared_ptr<Peer>)> onPeerDisconnected;
    std::function<void(std::shared_ptr<Block>)> onNewBlock;
    std::function<void(std::shared_ptr<Transaction>)> onNewTransaction;
    std::function<void(const std::string&)> onLog;
    std::function<void(int64_t)> onSyncProgress;
};

struct P2PConfig {
    uint16_t listen_port = 8333;
    size_t max_peers = 25;
    size_t min_peers = 3;
    int64_t ping_interval = 30000;
    int64_t peer_timeout = 90000;
    int64_t sync_interval = 60000;
    bool enable_logging = true;
    std::vector<std::string> seed_nodes;
};


class P2PNode {
private:
    std::string node_id;
    P2PConfig config;
    P2PCallbacks callbacks;
    Blockchain* blockchain;
    
    SocketType listen_socket;
    std::atomic<bool> running;
    std::atomic<bool> syncing;
    
    std::unordered_map<std::string, std::shared_ptr<Peer>> peers;
    mutable std::mutex peers_mutex;
    
    std::unordered_set<std::string> known_blocks;
    std::unordered_set<std::string> known_txs;
    mutable std::mutex known_mutex;
    
    std::jthread listener_thread;
    std::jthread receiver_thread;
    std::jthread ping_thread;
    std::jthread sync_thread;
    
    std::condition_variable stop_condition;
    mutable std::mutex stop_mutex;
    
public:
    P2PNode(Blockchain* chain, const P2PConfig& cfg = P2PConfig());
    ~P2PNode();
    
    P2PNode(const P2PNode&) = delete;
    P2PNode& operator=(const P2PNode&) = delete;
    
    bool start();
    void stop();
    bool isRunning() const { return running; }
    bool isSyncing() const { return syncing; }
    
    bool connectToPeer(const std::string& ip, uint16_t port);
    void disconnectPeer(const std::string& address);
    
    void broadcastBlock(std::shared_ptr<Block> block);
    void broadcastTransaction(std::shared_ptr<Transaction> tx);
    void requestSync();
    
    size_t getPeerCount() const;
    std::vector<PeerInfo> getConnectedPeers() const;
    const std::string& getNodeId() const { return node_id; }
    uint16_t getPort() const { return config.listen_port; }
    uint16_t getActualListenPort() const;
    
    void setCallbacks(const P2PCallbacks& cb) { callbacks = cb; }
    
private:
    std::string generateNodeId();
    bool initializeNetwork();
    void cleanupNetwork();
    bool createListenSocket();
    void acceptConnections();
    void receiveMessages();
    void pingPeers();
    void syncPeriodically();
    
    void handleMessage(std::shared_ptr<Peer> peer, const Message& msg);
    void handleHandshake(std::shared_ptr<Peer> peer, const Message& msg);
    void handlePing(std::shared_ptr<Peer> peer, const Message& msg);
    void handlePong(std::shared_ptr<Peer> peer, const Message& msg);
    void handleGetPeers(std::shared_ptr<Peer> peer, const Message& msg);
    void handlePeers(std::shared_ptr<Peer> peer, const Message& msg);
    void handleGetBlocks(std::shared_ptr<Peer> peer, const Message& msg);
    void handleBlocks(std::shared_ptr<Peer> peer, const Message& msg);
    void handleGetBlockHeight(std::shared_ptr<Peer> peer, const Message& msg);
    void handleBlockHeight(std::shared_ptr<Peer> peer, const Message& msg);
    void handleNewBlock(std::shared_ptr<Peer> peer, const Message& msg);
    void handleNewTransaction(std::shared_ptr<Peer> peer, const Message& msg);
    void handleDisconnect(std::shared_ptr<Peer> peer, const Message& msg);
    
    bool sendToPeer(std::shared_ptr<Peer> peer, const Message& msg);
    void broadcast(const Message& msg, const std::string& exclude_peer = "");
    void removePeer(const std::string& address);
    void checkPeerTimeouts();
    void connectToSeedNodes();
    std::shared_ptr<Peer> findBestPeerForSync();
    void syncWithPeer(std::shared_ptr<Peer> peer);
    void log(const std::string& message);
};

}