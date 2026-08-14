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
    // Uses a fixture key so the roundtrip is deterministic and reproducible;
    // the sign/verify contract is independent of key origin. (Random-scalar
    // keygen is only tens of milliseconds -- see wallet_test.cpp.)
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

// Issue #24: std::stoul skips leading whitespace, accepts a sign, and stops at
// the first invalid character without reporting it, so junk decoded as "valid"
// key and signature bytes.
TEST_CASE("hexToBytes rejects non-hex input", "[unit][eccrypto]") {
    uint8_t out[4] = {};

    REQUIRE(ECCrypto::hexToBytes(" 5", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("5 ", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("-1", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("+7", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("0x", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("az", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("g0", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("\t1", out, sizeof(out)) == 0);
}

TEST_CASE("hexToBytes decodes valid hex in either case", "[unit][eccrypto]") {
    uint8_t out[4] = {};
    REQUIRE(ECCrypto::hexToBytes("00ffAb10", out, sizeof(out)) == 4);
    REQUIRE(out[0] == 0x00);
    REQUIRE(out[1] == 0xff);
    REQUIRE(out[2] == 0xab);
    REQUIRE(out[3] == 0x10);
}

TEST_CASE("hexToBytes rejects odd-length and oversized input", "[unit][eccrypto]") {
    uint8_t out[4] = {};
    REQUIRE(ECCrypto::hexToBytes("abc", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("0011223344", out, sizeof(out)) == 0);
    REQUIRE(ECCrypto::hexToBytes("", out, sizeof(out)) == 0);
}
