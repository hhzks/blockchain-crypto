#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstring>
#include <set>
#include <string>
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

TEST_CASE("secureRandomBytes fills the whole buffer and varies per call",
          "[unit][eccrypto]") {
    uint8_t first[32];
    uint8_t second[32];
    std::memset(first, 0xAA, sizeof first);
    std::memset(second, 0xAA, sizeof second);

    ECCrypto::secureRandomBytes(first, sizeof first);
    ECCrypto::secureRandomBytes(second, sizeof second);

    // An untouched tail would still hold the sentinel.
    REQUIRE_FALSE(std::all_of(std::begin(first), std::end(first),
                              [](uint8_t b) { return b == 0xAA; }));
    REQUIRE(std::memcmp(first, second, sizeof first) != 0);
}

TEST_CASE("randomScalar draws distinct values inside [1, N-1]",
          "[unit][eccrypto]") {
    std::set<std::string> seen;

    for (int i = 0; i < 16; i++) {
        BigInt k = ECCrypto::randomScalar();
        REQUIRE(k > 0);
        REQUIRE(k < ECCrypto::secp256k1::N);
        seen.insert(k.to_string());
    }

    REQUIRE(seen.size() == 16);
}

namespace {

// A valid signature over `message`, so the range checks below are the only
// thing standing between a tampered copy and acceptance.
struct SignedFixture {
    std::unique_ptr<ECCrypto::KeyPair> kp;
    ECCrypto::PublicKey pub{};
    ECCrypto::Hash hash{};
    ECCrypto::Signature sig{};

    SignedFixture() {
        kp = ECCrypto::keyPairFromPrivateKeyHex(test_vectors::fixture_priv_hex);
        pub = ECCrypto::compressPublicKey(kp->public_key);
        hash = ECCrypto::sha256Hash("a message worth signing");
        sig = ECCrypto::signHash(hash, kp->private_key);
    }
};

} // namespace

TEST_CASE("verifySignature accepts an untampered signature", "[unit][eccrypto]") {
    SignedFixture f;
    REQUIRE(ECCrypto::verifySignature(f.hash, f.sig, f.pub));
}

TEST_CASE("verifySignature rejects R at the point at infinity",
          "[unit][eccrypto]") {
    // pointAtInfinity() is built as (0, 0) and getX() returns that 0 without
    // consulting is_infinity, so "computed R is the point at infinity" reads
    // as "R.x == 0". Choosing s = e*x makes sG - eP exactly infinity, and the
    // comparison against R.x = 0 then succeeds on a signature whose R is not a
    // point at all.
    SignedFixture f;

    uint8_t rx_bytes[32]{};                 // R.x = 0
    uint8_t pubx_bytes[32];
    ECCrypto::bigIntToBytes32(f.kp->public_key.getX(), pubx_bytes);

    std::vector<uint8_t> to_hash;
    to_hash.insert(to_hash.end(), std::begin(rx_bytes), std::end(rx_bytes));
    to_hash.insert(to_hash.end(), std::begin(pubx_bytes), std::end(pubx_bytes));
    to_hash.insert(to_hash.end(), f.hash.begin(), f.hash.end());

    ECCrypto::Hash e_hash = ECCrypto::sha256Hash(to_hash.data(), to_hash.size());
    BigInt e = ECCrypto::bytes32ToBigInt(e_hash.data()) % ECCrypto::secp256k1::N;
    BigInt s = (e * f.kp->private_key) % ECCrypto::secp256k1::N;

    ECCrypto::Signature forged{};
    ECCrypto::bigIntToBytes32(s, forged.data() + 32);   // R.x stays zero

    REQUIRE_FALSE(ECCrypto::verifySignature(f.hash, forged, f.pub));
}

TEST_CASE("verifySignature rejects out-of-range components",
          "[unit][eccrypto]") {
    // Hygiene rather than exploits: the arithmetic happens to reject these
    // today, but the invariant belongs where it is stated.
    SignedFixture f;

    SECTION("all-zero signature") {
        // signatureFromHex returns exactly this when the hex length is wrong,
        // so (0, 0) is a reachable input, not a hypothetical one.
        ECCrypto::Signature zero{};
        REQUIRE_FALSE(ECCrypto::verifySignature(f.hash, zero, f.pub));
    }

    SECTION("s = 0") {
        ECCrypto::Signature sig = f.sig;
        std::fill(sig.begin() + 32, sig.end(), uint8_t{0});
        REQUIRE_FALSE(ECCrypto::verifySignature(f.hash, sig, f.pub));
    }

    SECTION("s = N") {
        ECCrypto::Signature sig = f.sig;
        ECCrypto::bigIntToBytes32(ECCrypto::secp256k1::N, sig.data() + 32);
        REQUIRE_FALSE(ECCrypto::verifySignature(f.hash, sig, f.pub));
    }

    SECTION("R.x = P") {
        ECCrypto::Signature sig = f.sig;
        ECCrypto::bigIntToBytes32(ECCrypto::secp256k1::P, sig.data());
        REQUIRE_FALSE(ECCrypto::verifySignature(f.hash, sig, f.pub));
    }
}

TEST_CASE("decompressPublicKey rejects an unreduced x-coordinate",
          "[unit][eccrypto]") {
    // x = 1 is on the curve, and it is small enough that x + P still fits in
    // 32 bytes. Both encodings therefore decompress; storing x unreduced makes
    // them behave identically in arithmetic while comparing unequal and
    // deriving different addresses.
    ECCrypto::PublicKey reduced{};
    reduced[0] = ECCrypto::COMPRESSED_EVEN_PREFIX;
    ECCrypto::bigIntToBytes32(BigInt(1), reduced.data() + 1);

    ECCrypto::PublicKey unreduced{};
    unreduced[0] = ECCrypto::COMPRESSED_EVEN_PREFIX;
    ECCrypto::bigIntToBytes32(BigInt(1) + ECCrypto::secp256k1::P,
                              unreduced.data() + 1);

    REQUIRE(ECCrypto::isValidPublicKey(reduced));

    REQUIRE(ECCrypto::decompressPublicKey(unreduced).isPointAtInfinity());
    REQUIRE_FALSE(ECCrypto::isValidPublicKey(unreduced));
}

TEST_CASE("signHash refuses an out-of-range private key", "[unit][eccrypto]") {
    ECCrypto::Hash hash = ECCrypto::sha256Hash("anything");

    // Signing with 0 produced a signature over the point at infinity.
    ECCrypto::Signature zero_key = ECCrypto::signHash(hash, BigInt(0));
    REQUIRE(std::all_of(zero_key.begin(), zero_key.end(),
                        [](uint8_t b) { return b == 0; }));

    ECCrypto::Signature order_key =
        ECCrypto::signHash(hash, ECCrypto::secp256k1::N);
    REQUIRE(std::all_of(order_key.begin(), order_key.end(),
                        [](uint8_t b) { return b == 0; }));
}
