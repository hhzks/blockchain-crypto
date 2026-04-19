#include <catch2/catch_test_macros.hpp>
#include "Wallet.h"
#include "Transaction.h"
#include "fixtures.h"

using test_support::TempDir;

// Most tests use importPrivateKey with small scalars instead of
// generateNewAddress to keep runtime bounded — Wallet::generateNewAddress
// delegates to ECCrypto::generateKeyPair, whose naive BigInt scalar
// multiplication of a random 256-bit key takes minutes per call.

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
