#include <catch2/catch_test_macros.hpp>
#include "Wallet.h"
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
