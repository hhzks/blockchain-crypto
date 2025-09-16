#include "include/Blockchain.h"
#include "include/Transaction.h"
#include <iostream>
#include <memory>
#include <string>
#include <chrono>

void displayMenu() {
    std::cout << "\n=== BLOCKCHAIN DEMO ===" << std::endl;
    std::cout << "1. Add Transaction" << std::endl;
    std::cout << "2. Mine Block" << std::endl;
    std::cout << "3. Check Balance" << std::endl;
    std::cout << "4. Display Blockchain" << std::endl;
    std::cout << "5. Validate Blockchain" << std::endl;
    std::cout << "6. Save Blockchain" << std::endl;
    std::cout << "7. Load Blockchain" << std::endl;
    std::cout << "8. Run Demo" << std::endl;
    std::cout << "9. Exit" << std::endl;
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
                transaction->signTransaction(sender + "_private_key"); // Simplified signing
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
            
            case 9: {
                // Exit
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
