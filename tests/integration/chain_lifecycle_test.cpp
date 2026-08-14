#include <catch2/catch_test_macros.hpp>
#include "fixtures.h"
#include "Blockchain.h"
#include "Wallet.h"
#include "Transaction.h"

using test_support::TempDir;

// Uses importPrivateKey with small fixture scalars throughout so the test is
// deterministic. generateNewAddress is affordable too (tens of milliseconds
// since the 64-bit-limb BigInt rewrite) and is covered in wallet_test.cpp;
// mining, not ECC, is what dominates this test's runtime.
TEST_CASE("End-to-end: wallet -> tx -> mine -> validate -> save -> load",
          "[integration][lifecycle]") {
    wallet::Wallet w;
    REQUIRE(w.importPrivateKey(test_vectors::fixture_priv_hex));
    REQUIRE(w.importPrivateKey(
        "0000000000000000000000000000000000000000000000000000000000005678"));
    REQUIRE(w.importPrivateKey(
        "0000000000000000000000000000000000000000000000000000000000009abc"));
    auto addrs = w.getAllAddresses();
    std::string alice = addrs[0];
    std::string bob   = addrs[1];
    std::string miner = addrs[2];

    Blockchain chain(2, 50.0);

    auto fund_tx = std::make_shared<Transaction>("system", alice, 100.0);
    chain.addTransaction(fund_tx);
    chain.minePendingTransactions(miner);

    REQUIRE(chain.getBalance(alice) == 100.0);
    REQUIRE(chain.getBalance(bob) == 0.0);

    auto xfer = std::make_shared<Transaction>(alice, bob, 30.0);
    w.signTransaction(*xfer, alice);
    chain.addTransaction(xfer);
    chain.minePendingTransactions(miner);

    REQUIRE(chain.getBalance(alice) == 70.0);
    REQUIRE(chain.getBalance(bob) == 30.0);
    REQUIRE(chain.getBalance(miner) == 100.0);
    REQUIRE(chain.isChainValid());

    TempDir tmp;
    std::string path = tmp.file("e2e_chain.dat");
    REQUIRE(chain.saveToFile(path));

    Blockchain reloaded(2, 50.0);
    REQUIRE(reloaded.loadFromFile(path));
    REQUIRE(reloaded.getChainSize() == chain.getChainSize());
    REQUIRE(reloaded.getBalance(alice) == 70.0);
    REQUIRE(reloaded.getBalance(bob) == 30.0);
}
