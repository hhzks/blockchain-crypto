#include "include/Blockchain.h"
#include "include/Transaction.h"
#include "include/P2PNode.h"
#include "include/utils.h"
#include "include/Wallet.h"
#include "include/money.h"
#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <limits>
#include <optional>

#ifdef _WIN32
    #include <conio.h>
    #include <io.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

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

// Coin figures, entered as decimals and held as integer units.
std::optional<money::Amount> promptAmount(const std::string& prompt) {
    std::string line;
    while (promptLine(prompt, line)) {
        if (auto value = money::parse(line)) return value;
        std::cout << "Please enter a positive amount with at most 8 decimal places."
                  << std::endl;
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

// Reads a line without echoing it. A keystore password typed in the clear
// stays in the scrollback and in anything watching the terminal.
bool promptPassword(const std::string& prompt, std::string& out) {
    std::cout << prompt << std::flush;
    out.clear();

#ifdef _WIN32
    // _getch reads the console, not stdin, so without this a redirected or
    // piped run would block here forever.
    if (!_isatty(_fileno(stdin))) {
        return static_cast<bool>(std::getline(std::cin, out));
    }

    for (;;) {
        const int ch = _getch();
        if (ch == '\r' || ch == '\n') break;
        if (ch == 3) return false;               // Ctrl-C
        if (ch == 0 || ch == 224) { _getch(); continue; } // function/arrow key
        if (ch == '\b') {
            if (!out.empty()) out.pop_back();
            continue;
        }
        out.push_back(static_cast<char>(ch));
    }
    std::cout << std::endl;
    return true;
#else
    termios original{};
    if (tcgetattr(STDIN_FILENO, &original) != 0) {
        return static_cast<bool>(std::getline(std::cin, out)); // not a tty
    }

    termios quiet = original;
    quiet.c_lflag &= ~static_cast<tcflag_t>(ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &quiet);

    const bool ok = static_cast<bool>(std::getline(std::cin, out));

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
    std::cout << std::endl;
    return ok;
#endif
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
    std::cout << "--- Wallet ---" << std::endl;
    std::cout << "8.  Create New Address" << std::endl;
    std::cout << "9.  Import Private Key" << std::endl;
    std::cout << "10. List Addresses" << std::endl;
    std::cout << "11. Select Address" << std::endl;
    std::cout << "12. Save Wallet" << std::endl;
    std::cout << "13. Load Wallet" << std::endl;
    std::cout << "--- P2P Network ---" << std::endl;
    std::cout << "14. Start P2P Node" << std::endl;
    std::cout << "15. Stop P2P Node" << std::endl;
    std::cout << "16. Connect to Peer" << std::endl;
    std::cout << "17. Show Connected Peers" << std::endl;
    std::cout << "18. Request Blockchain Sync" << std::endl;
    std::cout << "19. P2P Node Status" << std::endl;
    std::cout << "-------------------" << std::endl;
    std::cout << "0.  Exit" << std::endl;
    std::cout << "Choice: ";
}

namespace {

// Prints the wallet's addresses with their on-chain balance, marking the one
// transactions are signed with.
void listAddresses(const wallet::Wallet& w, const Blockchain& blockchain) {
    const auto addresses = w.getAllAddresses();
    if (addresses.empty()) {
        std::cout << "Wallet is empty. Use 'Create New Address' or 'Load Wallet'."
                  << std::endl;
        return;
    }

    const std::string selected = w.getDefaultAddress();
    std::cout << "Addresses (" << addresses.size() << "):" << std::endl;
    for (const auto& address : addresses) {
        std::cout << "  " << address
                  << "  balance " << money::format(blockchain.getBalance(address))
                  << (address == selected ? "  [selected]" : "")
                  << std::endl;
    }
}

void createAddress(wallet::Wallet& w) {
    const std::string address = w.generateNewAddress();
    if (address.empty()) {
        std::cout << "Failed to generate an address." << std::endl;
        return;
    }

    std::cout << "Created " << address << std::endl;
    if (w.getDefaultAddress() == address) {
        std::cout << "Selected it for signing." << std::endl;
    }
    std::cout << "Save the wallet to keep this key." << std::endl;
}

void importPrivateKey(wallet::Wallet& w) {
    const auto priv_hex = promptText("Enter private key hex (64 chars): ");
    if (!priv_hex) return;

    if (!w.importPrivateKey(*priv_hex)) {
        std::cout << "Import failed. Expecting 64 hex characters of a valid key."
                  << std::endl;
        return;
    }

    std::cout << "Imported. Wallet now holds " << w.getAllAddresses().size()
              << " address(es)." << std::endl;
}

void selectAddress(wallet::Wallet& w) {
    if (w.getAllAddresses().empty()) {
        std::cout << "Wallet is empty." << std::endl;
        return;
    }

    const auto address = promptText("Enter address to select: ");
    if (!address) return;

    if (!w.hasAddress(*address)) {
        std::cout << "That address is not in this wallet." << std::endl;
        return;
    }

    w.setDefaultAddress(*address);
    std::cout << "Signing from " << *address << std::endl;
}

void saveWallet(const wallet::Wallet& w) {
    if (w.getAllAddresses().empty()) {
        std::cout << "Nothing to save: the wallet is empty." << std::endl;
        return;
    }

    const auto filename = promptText("Enter filename to save wallet: ");
    if (!filename) return;

    std::string password;
    if (!promptPassword("Password: ", password)) return;

    std::string confirmation;
    if (!promptPassword("Confirm password: ", confirmation)) return;

    if (password != confirmation) {
        std::cout << "Passwords do not match. Wallet not saved." << std::endl;
        return;
    }

    if (w.saveToFile(*filename, password)) {
        std::cout << "Wallet saved to " << *filename << std::endl;
    } else {
        std::cout << "Failed to save wallet." << std::endl;
    }
}

void loadWallet(wallet::Wallet& w) {
    const auto filename = promptText("Enter filename to load wallet: ");
    if (!filename) return;

    std::string password;
    if (!promptPassword("Password: ", password)) return;

    // A failed load leaves whatever the wallet already held untouched.
    if (!w.loadFromFile(*filename, password)) {
        std::cout << "Failed to load wallet (wrong password or corrupt file)."
                  << std::endl;
        return;
    }

    std::cout << "Loaded " << w.getAllAddresses().size() << " address(es)."
              << std::endl;
    if (!w.getDefaultAddress().empty()) {
        std::cout << "Signing from " << w.getDefaultAddress() << std::endl;
    }
}

void addTransaction(wallet::Wallet& w, Blockchain& blockchain) {
    const std::string sender = w.getDefaultAddress();
    if (sender.empty()) {
        std::cout << "No address selected. Use 'Create New Address', "
                     "'Import Private Key' or 'Load Wallet' first." << std::endl;
        return;
    }

    std::cout << "Sending from " << sender
              << " (balance " << money::format(blockchain.getBalance(sender))
              << ")" << std::endl;

    const auto receiver = promptText("Enter receiver address: ");
    if (!receiver) return;
    const auto amount = promptAmount("Enter amount: ");
    if (!amount) return;

    auto transaction = std::make_shared<Transaction>(sender, *receiver, *amount);
    if (!w.signTransaction(*transaction, sender)) {
        std::cout << "Failed to sign the transaction." << std::endl;
        return;
    }

    blockchain.addTransaction(transaction);
}

} // namespace

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

    // Keys live only in memory until "Save Wallet" writes an encrypted
    // keystore; nothing is loaded from disk without a password.
    wallet::Wallet user_wallet;

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
                addTransaction(user_wallet, blockchain);
                break;
            }

            case 2: {
                // A blank line mines to the wallet's selected address, so the
                // reward lands somewhere the operator can actually spend from.
                const std::string selected = user_wallet.getDefaultAddress();
                std::string line;
                if (!promptLine(selected.empty()
                                    ? "Enter miner address: "
                                    : "Enter miner address [" + selected + "]: ",
                                line)) {
                    break;
                }

                const std::string miner = isBlank(line) ? selected : line;
                if (miner.empty()) {
                    std::cout << "Please enter a miner address." << std::endl;
                    break;
                }

                blockchain.minePendingTransactions(miner);
                break;
            }

            case 3: {
                const auto address = promptText("Enter address to check: ");
                if (!address) break;
                const money::Amount balance = blockchain.getBalance(*address);
                std::cout << "Balance for " << *address << ": "
                          << money::format(balance) << std::endl;
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
                createAddress(user_wallet);
                break;
            }

            case 9: {
                importPrivateKey(user_wallet);
                break;
            }

            case 10: {
                listAddresses(user_wallet, blockchain);
                break;
            }

            case 11: {
                selectAddress(user_wallet);
                break;
            }

            case 12: {
                saveWallet(user_wallet);
                break;
            }

            case 13: {
                loadWallet(user_wallet);
                break;
            }

            case 14: {
                startP2PNode(blockchain);
                break;
            }
            
            case 15: {
                stopP2PNode();
                break;
            }
            
            case 16: {
                connectToPeer();
                break;
            }
            
            case 17: {
                showConnectedPeers();
                break;
            }
            
            case 18: {
                requestSync();
                break;
            }
            
            case 19: {
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
