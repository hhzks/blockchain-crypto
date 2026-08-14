#include "include/Blockchain.h"
#include "include/Transaction.h"
#include "include/P2PNode.h"
#include "include/utils.h"
#include "include/ECCrypto.h"
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <limits>
#include <optional>

// Global P2P node pointer
std::unique_ptr<p2p::P2PNode> p2pNode;

namespace {

// Every prompt reads a whole line. Mixing operator>> with std::getline leaves
// the newline behind and desynchronises the next read, and a failed operator>>
// value-initialises its target -- which is how junk at the menu used to select
// case 0 (Exit) and quit the program.
// Returns false only at end of input, so callers can shut down cleanly.
bool promptLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    return static_cast<bool>(std::getline(std::cin, out));
}

bool isBlank(const std::string& text) {
    return text.find_first_not_of(" \t\r\n\f\v") == std::string::npos;
}

std::optional<int> promptInt(const std::string& prompt) {
    std::string line;
    while (promptLine(prompt, line)) {
        if (auto value = utils::parseInt(line)) return value;
        std::cout << "Please enter a whole number." << std::endl;
    }
    return std::nullopt;
}

std::optional<double> promptDouble(const std::string& prompt) {
    std::string line;
    while (promptLine(prompt, line)) {
        if (auto value = utils::parseDouble(line)) return value;
        std::cout << "Please enter a number." << std::endl;
    }
    return std::nullopt;
}

// A blank line takes the default when one is offered.
std::optional<uint16_t> promptPort(const std::string& prompt,
                                   std::optional<uint16_t> default_port = std::nullopt) {
    std::string line;
    while (promptLine(prompt, line)) {
        if (isBlank(line) && default_port) return default_port;
        const auto value = utils::parseInt(line);
        if (value && *value >= 1 && *value <= 65535) {
            return static_cast<uint16_t>(*value);
        }
        std::cout << "Please enter a port between 1 and 65535." << std::endl;
    }
    return std::nullopt;
}

// Non-empty free text (an address, a filename).
std::optional<std::string> promptText(const std::string& prompt) {
    std::string line;
    while (promptLine(prompt, line)) {
        if (!isBlank(line)) return line;
        std::cout << "Please enter a value." << std::endl;
    }
    return std::nullopt;
}

} // namespace

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
        if (blockchain.addBlock(block)) {
            std::cout << "[P2P] Block #" << block->getIndex()
                      << " accepted" << std::endl;
        } else {
            std::cout << "[P2P] Block #" << block->getIndex()
                      << " rejected" << std::endl;
        }
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
    
    const auto port = promptPort("Enter port to listen on (default 8333): ", uint16_t{8333});
    if (!port) return;

    p2p::P2PConfig config;
    config.listen_port = *port;
    config.max_peers = 25;
    config.min_peers = 3;
    config.enable_logging = true;

    std::string answer;
    if (!promptLine("Add seed node? (y/n): ", answer)) return;

    while (!answer.empty() && (answer[0] == 'y' || answer[0] == 'Y')) {
        const auto seed_ip = promptText("Enter seed node IP: ");
        if (!seed_ip) return;
        const auto seed_port = promptPort("Enter seed node port: ");
        if (!seed_port) return;

        config.seed_nodes.push_back(*seed_ip + ":" + std::to_string(*seed_port));

        if (!promptLine("Add another seed node? (y/n): ", answer)) return;
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
    
    const auto ip = promptText("Enter peer IP address: ");
    if (!ip) return;
    const auto port = promptPort("Enter peer port: ");
    if (!port) return;

    if (p2pNode->connectToPeer(*ip, *port)) {
        std::cout << "Connection initiated to " << *ip << ":" << *port << std::endl;
    } else {
        std::cout << "Failed to connect to " << *ip << ":" << *port << std::endl;
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

    auto shutdown = []() {
        if (p2pNode && p2pNode->isRunning()) {
            std::cout << "Stopping P2P node..." << std::endl;
            p2pNode->stop();
        }
        std::cout << "Goodbye!" << std::endl;
    };

    while (true) {
        displayMenu();

        std::string line;
        if (!std::getline(std::cin, line)) {
            // Real end of input (Ctrl-D / Ctrl-Z / closed pipe), not junk.
            std::cout << std::endl;
            shutdown();
            return 0;
        }

        const auto parsed = utils::parseInt(line);
        if (!parsed) {
            std::cout << "Invalid choice! Please try again." << std::endl;
            continue;
        }

        switch (*parsed) {
            case 1: {
                const auto sender = promptText("Enter sender address: ");
                if (!sender) break;
                const auto receiver = promptText("Enter receiver address: ");
                if (!receiver) break;
                const auto parsed_amount = promptDouble("Enter amount: ");
                if (!parsed_amount) break;
                const double amount = *parsed_amount;

                // Demo key management: derive a deterministic private key from
                // the sender name, then transact from the address that key
                // actually controls. isValid() now binds the signature to
                // deriveAddress(pubkey), so a free-text sender is rejected.
                std::string demo_priv = utils::sha256(*sender + "_private_key");
                auto demo_kp = ECCrypto::keyPairFromPrivateKeyHex(demo_priv);
                if (!demo_kp) {
                    std::cout << "Failed to derive a key for sender '" << *sender
                              << "'" << std::endl;
                    break;
                }
                std::cout << "Using address " << demo_kp->address
                          << " for '" << *sender << "'" << std::endl;
                auto transaction = std::make_shared<Transaction>(
                    demo_kp->address, *receiver, amount);
                transaction->signTransaction(demo_priv);
                blockchain.addTransaction(transaction);
                break;
            }
            
            case 2: {
                const auto minerAddress = promptText("Enter miner address: ");
                if (!minerAddress) break;
                blockchain.minePendingTransactions(*minerAddress);
                break;
            }

            case 3: {
                const auto address = promptText("Enter address to check: ");
                if (!address) break;
                double balance = blockchain.getBalance(*address);
                std::cout << "Balance for " << *address << ": " << balance << std::endl;
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
                const auto filename = promptText("Enter filename to save: ");
                if (!filename) break;
                blockchain.saveToFile(*filename);
                break;
            }

            case 7: {
                const auto filename = promptText("Enter filename to load: ");
                if (!filename) break;
                blockchain.loadFromFile(*filename);
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
                shutdown();
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
