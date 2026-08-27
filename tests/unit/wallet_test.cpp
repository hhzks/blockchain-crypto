#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <iterator>
#include "Wallet.h"
#include "keystore.h"
#include "Blockchain.h"
#include "Transaction.h"
#include "ECCrypto.h"
#include "fixtures.h"

using test_support::TempDir;

// Most tests use importPrivateKey with small fixture scalars, which keeps them
// fast and deterministic. generateNewAddress is covered separately below: since
// the 64-bit-limb BigInt rewrite a random-key keygen costs on the order of
// 20-100 ms, not the minutes an earlier comment here claimed.

TEST_CASE("importPrivateKey derives matching address", "[unit][wallet]") {
    wallet::Wallet w;
    REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
    auto all = w.getAllAddresses();
    REQUIRE(all.size() == 1);
    REQUIRE_FALSE(w.getPublicKeyHex(all[0]).empty());
    REQUIRE(w.hasAddress(all[0]));
}

TEST_CASE("importPrivateKey yields distinct addresses for distinct keys",
          "[unit][wallet]") {
    wallet::Wallet w;
    REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
    REQUIRE(w.importPrivateKey(
        "0000000000000000000000000000000000000000000000000000000000005678"));
    auto all = w.getAllAddresses();
    REQUIRE(all.size() == 2);
    REQUIRE(all[0] != all[1]);
}

TEST_CASE("signTransaction + verifyTransaction roundtrip", "[unit][wallet]") {
    wallet::Wallet w;
    REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
    std::string addr = w.getAllAddresses()[0];

    Transaction t(addr, "receiver", 1.0);
    REQUIRE(w.signTransaction(t, addr));
    REQUIRE(w.verifyTransaction(t, addr));
}

TEST_CASE("signTransaction rejects mismatched sender", "[unit][wallet]") {
    wallet::Wallet w;
    REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
    REQUIRE(w.importPrivateKey(
        "0000000000000000000000000000000000000000000000000000000000005678"));
    auto addrs = w.getAllAddresses();
    Transaction t(addrs[1], "receiver", 1.0);
    REQUIRE_FALSE(w.signTransaction(t, addrs[0]));
}

TEST_CASE("saveToFile/loadFromFile preserves addresses", "[unit][wallet]") {
    TempDir tmp;
    std::string path = tmp.file("wallet.dat");
    std::string addr;
    {
        wallet::Wallet w;
        REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
        addr = w.getAllAddresses()[0];
        REQUIRE(w.saveToFile(path, ""));
    }
    wallet::Wallet loaded;
    REQUIRE(loaded.loadFromFile(path, ""));
    REQUIRE(loaded.hasAddress(addr));
}

// Issue #29: generateNewAddress is the call a real user makes and no test
// exercised it, on the strength of a stale "minutes per call" claim.
TEST_CASE("generateNewAddress produces a usable, self-consistent keypair",
          "[unit][wallet]") {
    wallet::Wallet w;

    const std::string address = w.generateNewAddress();
    REQUIRE_FALSE(address.empty());
    REQUIRE(w.hasAddress(address));

    const std::string priv = w.getPrivateKeyHex(address);
    const std::string pub = w.getPublicKeyHex(address);
    REQUIRE(priv.length() == ECCrypto::PRIVATE_KEY_SIZE * 2);
    REQUIRE(pub.length() == ECCrypto::PUBLIC_KEY_SIZE * 2);

    // The generated key must actually control the address it was filed under.
    ECCrypto::PublicKey pub_bytes{};
    REQUIRE(ECCrypto::hexToBytes(pub, pub_bytes.data(), pub_bytes.size())
            == ECCrypto::PUBLIC_KEY_SIZE);
    REQUIRE(ECCrypto::deriveAddress(pub_bytes) == address);
}

TEST_CASE("generateNewAddress yields a distinct address each call",
          "[unit][wallet]") {
    wallet::Wallet w;
    const std::string first = w.generateNewAddress();
    const std::string second = w.generateNewAddress();
    REQUIRE_FALSE(first.empty());
    REQUIRE_FALSE(second.empty());
    REQUIRE(first != second);
    REQUIRE(w.getAllAddresses().size() == 2);
}

TEST_CASE("a generated key signs a transaction that validates end to end",
          "[unit][wallet]") {
    wallet::Wallet w;
    const std::string address = w.generateNewAddress();

    Transaction tx(address, "receiver", 5.0);
    REQUIRE(tx.signTransaction(w.getPrivateKeyHex(address)));
    tx.setSenderPublicKey(w.getPublicKeyHex(address));
    REQUIRE(tx.isValid());
}

TEST_CASE("wallet::utils::isValidAddress accepts a derived address",
          "[unit][wallet]") {
    wallet::Wallet w;
    const std::string address = w.generateNewAddress();

    REQUIRE(wallet::utils::isValidAddress(address));
}

TEST_CASE("wallet::utils::isValidAddress rejects malformed addresses",
          "[unit][wallet]") {
    // deriveAddress returns the first 20 bytes of SHA-256 as lowercase hex.
    REQUIRE_FALSE(wallet::utils::isValidAddress(""));
    REQUIRE_FALSE(wallet::utils::isValidAddress(std::string(39, 'a')));
    REQUIRE_FALSE(wallet::utils::isValidAddress(std::string(41, 'a')));
    REQUIRE_FALSE(wallet::utils::isValidAddress(std::string(40, 'g')));
    REQUIRE_FALSE(wallet::utils::isValidAddress(std::string(40, 'A')));
    REQUIRE_FALSE(wallet::utils::isValidAddress("0x" + std::string(40, 'a')));
}

TEST_CASE("wallet::utils::createRandomWallet yields a usable default address",
          "[unit][wallet]") {
    auto w = wallet::utils::createRandomWallet();
    REQUIRE(w != nullptr);

    const std::string address = w->getDefaultAddress();
    REQUIRE(wallet::utils::isValidAddress(address));
    REQUIRE(w->hasAddress(address));
    REQUIRE(w->getAllAddresses().size() == 1);

    // Two wallets must not share a key.
    auto other = wallet::utils::createRandomWallet();
    REQUIRE(other->getDefaultAddress() != address);
}

TEST_CASE("saved keystores do not contain the private key in the clear",
          "[unit][wallet]") {
    TempDir tmp;
    const std::string path = tmp.file("encrypted.dat");

    wallet::Wallet w;
    REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
    REQUIRE(w.saveToFile(path, "a good password"));

    std::ifstream in(path, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());

    REQUIRE_FALSE(contents.empty());
    REQUIRE(contents.find(test_vectors::fixture_priv_hex) == std::string::npos);
    REQUIRE(contents.find(w.getAllAddresses()[0]) == std::string::npos);
}

TEST_CASE("a keystore round-trips under its own password", "[unit][wallet]") {
    TempDir tmp;
    const std::string path = tmp.file("roundtrip.dat");

    std::string address;
    {
        wallet::Wallet w;
        REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
        address = w.getAllAddresses()[0];
        REQUIRE(w.saveToFile(path, "pw"));
    }

    wallet::Wallet loaded;
    REQUIRE(loaded.loadFromFile(path, "pw"));
    REQUIRE(loaded.hasAddress(address));
    REQUIRE(loaded.getPrivateKeyHex(address) == test_vectors::fixture_priv_hex);
    REQUIRE(loaded.getDefaultAddress() == address);
}

TEST_CASE("loading with the wrong password fails and keeps the existing keys",
          "[unit][wallet]") {
    TempDir tmp;
    const std::string path = tmp.file("wrongpw.dat");

    {
        wallet::Wallet w;
        REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
        REQUIRE(w.saveToFile(path, "right"));
    }

    wallet::Wallet holder;
    const std::string kept = holder.generateNewAddress();
    REQUIRE_FALSE(kept.empty());

    REQUIRE_FALSE(holder.loadFromFile(path, "wrong"));
    // A failed load must not throw away what the wallet already held.
    REQUIRE(holder.hasAddress(kept));
}

TEST_CASE("loadFromFile reports failure when an entry cannot be imported",
          "[unit][wallet]") {
    TempDir tmp;
    const std::string path = tmp.file("corrupt.dat");

    wallet::Wallet w;
    REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
    REQUIRE(w.saveToFile(path, "pw"));

    // Re-encrypt a body whose key material is not importable. Previously every
    // line could fail and loadFromFile still returned true.
    const std::string broken_body =
        "DEFAULT:abc\nabc:notavalidkey:notavalidkey\n";
    std::ofstream out(path, std::ios::binary);
    out << keystore::encrypt(broken_body, "pw");
    out.close();

    wallet::Wallet loaded;
    REQUIRE_FALSE(loaded.loadFromFile(path, "pw"));
}

TEST_CASE("a wallet-signed transaction is accepted by the chain",
          "[unit][wallet]") {
    // The contract the CLI leans on once it signs from the wallet instead of
    // a key derived from the sender's name: what the wallet signs must pass
    // Transaction::isValid and reach the mempool.
    test_support::MinedChainFixture f;

    wallet::Wallet w;
    const std::string address = w.generateNewAddress();
    REQUIRE_FALSE(address.empty());
    REQUIRE(w.getDefaultAddress() == address);

    f.seedFunds(address, 100.0, "miner_1");

    auto tx = std::make_shared<Transaction>(address, "receiver", 30.0);
    REQUIRE(w.signTransaction(*tx, address));
    REQUIRE(tx->isValid());

    f.chain.addTransaction(tx);
    REQUIRE(f.chain.getPendingTransactions().size() == 1);

    f.chain.minePendingTransactions("miner_1");
    REQUIRE(f.chain.getBalance(address) == 70.0);
    REQUIRE(f.chain.getBalance("receiver") == 30.0);
}
