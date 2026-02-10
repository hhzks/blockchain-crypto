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
    std::cout << "\n=== BLOCKCHAIN DEMO ===" << std::endl;
    std::cout << "1.  Add Transaction" << std::endl;
    std::cout << "2.  Mine Block" << std::endl;
    std::cout << "3.  Check Balance" << std::endl;
    std::cout << "4.  Display Blockchain" << std::endl;
    std::cout << "5.  Validate Blockchain" << std::endl;
    std::cout << "6.  Save Blockchain" << std::endl;
    std::cout << "7.  Load Blockchain" << std::endl;
    std::cout << "8.  Run Demo" << std::endl;
    std::cout << "--- P2P Network ---" << std::endl;
    std::cout << "10. Start P2P Node" << std::endl;
    std::cout << "11. Stop P2P Node" << std::endl;
    std::cout << "12. Connect to Peer" << std::endl;
    std::cout << "13. Show Connected Peers" << std::endl;
    std::cout << "14. Request Blockchain Sync" << std::endl;
    std::cout << "15. P2P Node Status" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << "0.  Exit" << std::endl;
    std::cout << "Choice: ";
}

void runDemo(Blockchain& blockchain) {
    std::cout << "\n=== RUNNING BLOCKCHAIN DEMO ===" << std::endl;
    
    // Create some sample transactions
    std::cout << "Creating sample transactions..." << std::endl;
    
    auto tx1 = std::make_shared<Transaction>("Alice", "Bob", 50.0);
    tx1->signTransaction("alice_private_key");
    blockchain.addTransaction(tx1);
    
    auto tx2 = std::make_shared<Transaction>("Bob", "Charlie", 25.0);
    tx2->signTransaction("bob_private_key");
    blockchain.addTransaction(tx2);
    
    auto tx3 = std::make_shared<Transaction>("Charlie", "Alice", 10.0);
    tx3->signTransaction("charlie_private_key");
    blockchain.addTransaction(tx3);
    
    // Mine a block
    std::cout << "\nMining block with transactions..." << std::endl;
    blockchain.minePendingTransactions("Miner1");
    
    // Add more transactions
    auto tx4 = std::make_shared<Transaction>("Alice", "David", 15.0);
    tx4->signTransaction("alice_private_key");
    blockchain.addTransaction(tx4);
    
    auto tx5 = std::make_shared<Transaction>("Bob", "Eve", 30.0);
    tx5->signTransaction("bob_private_key");
    blockchain.addTransaction(tx5);
    
    // Mine another block
    std::cout << "\nMining second block..." << std::endl;
    blockchain.minePendingTransactions("Miner2");
    
    // Display results
    std::cout << "\n=== DEMO RESULTS ===" << std::endl;
    std::cout << "Blockchain validation: " << (blockchain.isChainValid() ? "VALID" : "INVALID") << std::endl;
    
    std::cout << "\nAccount Balances:" << std::endl;
    std::vector<std::string> accounts = {"Alice", "Bob", "Charlie", "David", "Eve", "Miner1", "Miner2"};
    for (const auto& account : accounts) {
        double balance = blockchain.getBalance(account);
        if (balance != 0) {
            std::cout << account << ": " << balance << std::endl;
        }
    }
    
    std::cout << "\nDemo completed!" << std::endl;
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
    config.listenPort = port;
    config.maxPeers = 25;
    config.minPeers = 3;
    config.enableLogging = true;
    
    // Optional: Add seed nodes
    std::cout << "Add seed node? (y/n): ";
    char addSeed;
    std::cin >> addSeed;
    
    while (addSeed == 'y' || addSeed == 'Y') {
        std::string seedIp;
        uint16_t seedPort;
        
        std::cout << "Enter seed node IP: ";
        std::cin >> seedIp;
        std::cout << "Enter seed node port: ";
        std::cin >> seedPort;
        
        config.seedNodes.push_back(seedIp + ":" + std::to_string(seedPort));
        
        std::cout << "Add another seed node? (y/n): ";
        std::cin >> addSeed;
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
            if (!peer.nodeId.empty()) {
                std::cout << " (ID: " << peer.nodeId.substr(0, 8) << "...)";
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
    
    // Create blockchain
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
                // Add Transaction
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
                // Mine Block
                std::string minerAddress;
                std::cout << "Enter miner address: ";
                std::cin >> minerAddress;
                blockchain.minePendingTransactions(minerAddress);
                break;
            }
            
            case 3: {
                // Check Balance
                std::string address;
                std::cout << "Enter address to check: ";
                std::cin >> address;
                double balance = blockchain.getBalance(address);
                std::cout << "Balance for " << address << ": " << balance << std::endl;
                break;
            }
            
            case 4: {
                // Display Blockchain
                blockchain.printChain();
                break;
            }
            
            case 5: {
                // Validate Blockchain
                bool isValid = blockchain.isChainValid();
                std::cout << "Blockchain is " << (isValid ? "VALID" : "INVALID") << std::endl;
                break;
            }
            
            case 6: {
                // Save Blockchain
                std::string filename;
                std::cout << "Enter filename to save: ";
                std::cin >> filename;
                blockchain.saveToFile(filename);
                break;
            }
            
            case 7: {
                // Load Blockchain
                std::string filename;
                std::cout << "Enter filename to load: ";
                std::cin >> filename;
                blockchain.loadFromFile(filename);
                break;
            }
            
            case 8: {
                // Run Demo
                runDemo(blockchain);
                break;
            }
            
            // P2P Network options
            case 10: {
                // Start P2P Node
                startP2PNode(blockchain);
                break;
            }
            
            case 11: {
                // Stop P2P Node
                stopP2PNode();
                break;
            }
            
            case 12: {
                // Connect to Peer
                connectToPeer();
                break;
            }
            
            case 13: {
                // Show Connected Peers
                showConnectedPeers();
                break;
            }
            
            case 14: {
                // Request Blockchain Sync
                requestSync();
                break;
            }
            
            case 15: {
                // P2P Node Status
                showP2PStatus();
                break;
            }
            
            case 0: {
                // Exit
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
