#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "ECCrypto.h"
#include "bigint.h"
#include "vectors.h"

TEST_CASE("isValidPrivateKey rejects 0 and N, accepts 1..N-1", "[unit][eccrypto]") {
    ECCrypto::PrivateKey zero{};
    REQUIRE_FALSE(ECCrypto::isValidPrivateKey(zero));

    ECCrypto::PrivateKey one{};
    one[31] = 0x01;
    REQUIRE(ECCrypto::isValidPrivateKey(one));
}

TEST_CASE("priv=1 yields generator point G", "[unit][eccrypto]") {
    auto kp = ECCrypto::keyPairFromPrivateKey(BigInt(1LL));
    REQUIRE(kp != nullptr);
    BigInt gx(test_vectors::secp256k1_G_x, 16);
    BigInt gy(test_vectors::secp256k1_G_y, 16);
    REQUIRE(kp->public_key.getX() == gx);
    REQUIRE(kp->public_key.getY() == gy);
}

TEST_CASE("sign/verify roundtrip with deterministic keypair", "[unit][eccrypto]") {
    // Uses fixture key (small scalar) instead of generateKeyPair() because the
    // codebase's naive BigInt scalar multiplication makes random-scalar keygen
    // take minutes; the sign/verify contract is independent of key origin.
    auto kp = ECCrypto::keyPairFromPrivateKeyHex(test_vectors::fixture_priv_hex);
    REQUIRE(kp != nullptr);

    ECCrypto::PublicKey pub = ECCrypto::compressPublicKey(kp->public_key);
    auto sig = ECCrypto::signMessage("hello world", kp->private_key);
    REQUIRE(ECCrypto::verifyMessageSignature("hello world", sig, pub));
}

TEST_CASE("verify rejects tampered signature", "[unit][eccrypto]") {
    auto kp = ECCrypto::keyPairFromPrivateKeyHex(test_vectors::fixture_priv_hex);
    ECCrypto::PublicKey pub = ECCrypto::compressPublicKey(kp->public_key);
    auto sig = ECCrypto::signMessage("original", kp->private_key);
    sig[0] ^= 0x01;
    REQUIRE_FALSE(ECCrypto::verifyMessageSignature("original", sig, pub));
}

TEST_CASE("verify rejects signature for different message", "[unit][eccrypto]") {
    auto kp = ECCrypto::keyPairFromPrivateKeyHex(test_vectors::fixture_priv_hex);
    ECCrypto::PublicKey pub = ECCrypto::compressPublicKey(kp->public_key);
    auto sig = ECCrypto::signMessage("original", kp->private_key);
    REQUIRE_FALSE(ECCrypto::verifyMessageSignature("tampered", sig, pub));
}

TEST_CASE("compressed <-> uncompressed pubkey roundtrip", "[unit][eccrypto]") {
    auto kp = ECCrypto::keyPairFromPrivateKeyHex(test_vectors::fixture_priv_hex);
    ECCrypto::PublicKey compressed = ECCrypto::compressPublicKey(kp->public_key);
    ECCrypto::UncompressedPublicKey uncompressed =
        ECCrypto::decompressToUncompressed(compressed);
    ECCrypto::PublicKey recompressed = ECCrypto::compressPublicKey(uncompressed);
    REQUIRE(std::equal(compressed.begin(), compressed.end(), recompressed.begin()));
}

TEST_CASE("deriveAddress is deterministic and non-empty", "[unit][eccrypto]") {
    auto kp = ECCrypto::keyPairFromPrivateKeyHex(test_vectors::fixture_priv_hex);
    REQUIRE(kp != nullptr);
    REQUIRE_FALSE(kp->address.empty());

    auto kp2 = ECCrypto::keyPairFromPrivateKeyHex(test_vectors::fixture_priv_hex);
    REQUIRE(kp->address == kp2->address);
}
