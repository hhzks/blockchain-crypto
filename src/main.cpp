#include "include/Blockchain.h"
#include "include/Transaction.h"
#include "include/P2PNode.h"
#include <iostream>
#include <memory>
#include <string>
#include <chrono>

// Global P2P node pointer
std::unique_ptr<p2p::P2PNode> p2pNode;

void displayMenu() {
    std::cout << "\n=== LOCAL BLOCKCHAIN ===" << std::endl;
    std::cout << "1.  Add Transaction" << std::endl;
    std::cout << "2.  Mine Block" << std::endl;
    std::cout << "3.  Check Balance" << std::endl;
    std::cout << "4.  Display Blockchain" << std::endl;
    std::cout << "5.  Validate Blockchain" << std::endl;
    std::cout << "6.  Save Blockchain" << std::endl;
    std::cout << "7.  Load Blockchain" << std::endl;
    std::cout << "--- P2P Network ---" << std::endl;
    std::cout << "8. Start P2P Node" << std::endl;
    std::cout << "9. Stop P2P Node" << std::endl;
    std::cout << "10. Connect to Peer" << std::endl;
    std::cout << "11. Show Connected Peers" << std::endl;
    std::cout << "12. Request Blockchain Sync" << std::endl;
    std::cout << "13. P2P Node Status" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << "0.  Exit" << std::endl;
    std::cout << "Choice: ";
}

void setupP2PCallbacks(Blockchain& blockchain) {
    if (!p2pNode) return;
    
    p2p::P2PCallbacks callbacks;
    
    callbacks.onPeerConnected = [](std::shared_ptr<p2p::Peer> peer) {
        std::cout << "\n[P2P] Peer connected: " << peer->getAddress() << std::endl;
    };
    
    callbacks.onPeerDisconnected = [](std::shared_ptr<p2p::Peer> peer) {
        std::cout << "\n[P2P] Peer disconnected: " << peer->getAddress() << std::endl;
    };
    
    callbacks.onNewBlock = [&blockchain](std::shared_ptr<Block> block) {
        std::cout << "\n[P2P] Received new block #" << block->getIndex() << std::endl;
        // In a real implementation, you would validate and add the block
        // blockchain.addBlock(block);
    };
    
    callbacks.onNewTransaction = [&blockchain](std::shared_ptr<Transaction> tx) {
        std::cout << "\n[P2P] Received new transaction: " 
                  << tx->getSender() << " -> " << tx->getReceiver() 
                  << " (" << tx->getAmount() << ")" << std::endl;
        blockchain.addTransaction(tx);
    };
    
    callbacks.onSyncProgress = [](int64_t progress) {
        std::cout << "\r[P2P] Sync progress: " << progress << "%" << std::flush;
        if (progress >= 100) std::cout << std::endl;
    };
    
    p2pNode->setCallbacks(callbacks);
}

void startP2PNode(Blockchain& blockchain) {
    if (p2pNode && p2pNode->isRunning()) {
        std::cout << "P2P node is already running!" << std::endl;
        return;
    }
    
    uint16_t port;
    std::cout << "Enter port to listen on (default 8333): ";
    std::string portStr;
    std::cin >> portStr;
    
    try {
        port = portStr.empty() ? 8333 : static_cast<uint16_t>(std::stoi(portStr));
    } catch (...) {
        port = 8333;
    }
    
    p2p::P2PConfig config;
    config.listen_port = port;
    config.max_peers = 25;
    config.min_peers = 3;
    config.enable_logging = true;
    
    std::cout << "Add seed node? (y/n): ";
    char add_seed;
    std::cin >> add_seed;
    
    while (add_seed == 'y' || add_seed == 'Y') {
        std::string seed_ip;
        uint16_t seed_port;
        
        std::cout << "Enter seed node IP: ";
        std::cin >> seed_ip;
        std::cout << "Enter seed node port: ";
        std::cin >> seed_port;
        
        config.seed_nodes.push_back(seed_ip + ":" + std::to_string(seed_port));
        
        std::cout << "Add another seed node? (y/n): ";
        std::cin >> add_seed;
    }
    
    p2pNode = std::make_unique<p2p::P2PNode>(&blockchain, config);
    setupP2PCallbacks(blockchain);
    
    if (p2pNode->start()) {
        std::cout << "P2P node started successfully!" << std::endl;
        std::cout << "Node ID: " << p2pNode->getNodeId() << std::endl;
        std::cout << "Listening on port: " << p2pNode->getPort() << std::endl;
    } else {
        std::cout << "Failed to start P2P node!" << std::endl;
        p2pNode.reset();
    }
}

void stopP2PNode() {
    if (!p2pNode || !p2pNode->isRunning()) {
        std::cout << "P2P node is not running!" << std::endl;
        return;
    }
    
    p2pNode->stop();
    p2pNode.reset();
    std::cout << "P2P node stopped." << std::endl;
}

void connectToPeer() {
    if (!p2pNode || !p2pNode->isRunning()) {
        std::cout << "P2P node is not running! Start it first." << std::endl;
        return;
    }
    
    std::string ip;
    uint16_t port;
    
    std::cout << "Enter peer IP address: ";
    std::cin >> ip;
    std::cout << "Enter peer port: ";
    std::cin >> port;
    
    if (p2pNode->connectToPeer(ip, port)) {
        std::cout << "Connection initiated to " << ip << ":" << port << std::endl;
    } else {
        std::cout << "Failed to connect to " << ip << ":" << port << std::endl;
    }
}

void showConnectedPeers() {
    if (!p2pNode || !p2pNode->isRunning()) {
        std::cout << "P2P node is not running!" << std::endl;
        return;
    }
    
    auto peers = p2pNode->getConnectedPeers();
    
    std::cout << "\n=== Connected Peers (" << peers.size() << ") ===" << std::endl;
    if (peers.empty()) {
        std::cout << "No peers connected." << std::endl;
    } else {
        for (const auto& peer : peers) {
            std::cout << "  - " << peer.ip << ":" << peer.port;
            if (!peer.node_id.empty()) {
                std::cout << " (ID: " << peer.node_id.substr(0, 8) << "...)";
            }
            std::cout << std::endl;
        }
    }
}

void requestSync() {
    if (!p2pNode || !p2pNode->isRunning()) {
        std::cout << "P2P node is not running!" << std::endl;
        return;
    }
    
    std::cout << "Requesting blockchain sync from peers..." << std::endl;
    p2pNode->requestSync();
}

void showP2PStatus() {
    if (!p2pNode) {
        std::cout << "P2P node not initialized." << std::endl;
        return;
    }
    
    std::cout << "\n=== P2P Node Status ===" << std::endl;
    std::cout << "Running: " << (p2pNode->isRunning() ? "Yes" : "No") << std::endl;
    std::cout << "Node ID: " << p2pNode->getNodeId() << std::endl;
    std::cout << "Port: " << p2pNode->getPort() << std::endl;
    std::cout << "Connected peers: " << p2pNode->getPeerCount() << std::endl;
    std::cout << "Syncing: " << (p2pNode->isSyncing() ? "Yes" : "No") << std::endl;
}

int main() {
    std::cout << "Blockchain Implementation in C++" << std::endl;
    std::cout << "=================================" << std::endl;
    
    Blockchain blockchain;
    
    int choice;
    std::string input;
    
    while (true) {
        displayMenu();

        if (std::cin.fail()){
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
        std::cin >> choice;

        switch (choice) {
            case 1: {
                std::string sender, receiver;
                double amount;
                
                std::cout << "Enter sender address: ";
                std::cin >> sender;
                std::cout << "Enter receiver address: ";
                std::cin >> receiver;
                std::cout << "Enter amount: ";
                std::cin >> amount;
                
                auto transaction = std::make_shared<Transaction>(sender, receiver, amount);
                transaction->signTransaction(sender + "_private_key");
                blockchain.addTransaction(transaction);
                break;
            }
            
            case 2: {
                std::string minerAddress;
                std::cout << "Enter miner address: ";
                std::cin >> minerAddress;
                blockchain.minePendingTransactions(minerAddress);
                break;
            }
            
            case 3: {
                std::string address;
                std::cout << "Enter address to check: ";
                std::cin >> address;
                double balance = blockchain.getBalance(address);
                std::cout << "Balance for " << address << ": " << balance << std::endl;
                break;
            }
            
            case 4: {
                blockchain.printChain();
                break;
            }
            
            case 5: {
                bool isValid = blockchain.isChainValid();
                std::cout << "Blockchain is " << (isValid ? "VALID" : "INVALID") << std::endl;
                break;
            }
            
            case 6: {
                std::string filename;
                std::cout << "Enter filename to save: ";
                std::cin >> filename;
                blockchain.saveToFile(filename);
                break;
            }
            
            case 7: {
                std::string filename;
                std::cout << "Enter filename to load: ";
                std::cin >> filename;
                blockchain.loadFromFile(filename);
                break;
            }
            
            
            case 8: {
                startP2PNode(blockchain);
                break;
            }
            
            case 9: {
                stopP2PNode();
                break;
            }
            
            case 10: {
                connectToPeer();
                break;
            }
            
            case 11: {
                showConnectedPeers();
                break;
            }
            
            case 12: {
                requestSync();
                break;
            }
            
            case 13: {
                showP2PStatus();
                break;
            }
            
            case 0: {
                if (p2pNode && p2pNode->isRunning()) {
                    std::cout << "Stopping P2P node..." << std::endl;
                    p2pNode->stop();
                }
                std::cout << "Goodbye!" << std::endl;
                return 0;
            }
            
            default: {
                std::cout << "Invalid choice! Please try again." << std::endl;
                break;
            }
        }
    }
    
    return 0;
}
