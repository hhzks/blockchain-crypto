#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include "keystore.h"
#include "ECCrypto.h"

namespace {

// The construction is what these tests check; the cost of the KDF is pinned
// by the PBKDF2 vectors above. Production uses keystore::DEFAULT_ITERATIONS.
constexpr uint32_t test_iterations = 1000;

std::vector<uint8_t> repeated(uint8_t byte, size_t count) {
    return std::vector<uint8_t>(count, byte);
}

std::vector<uint8_t> bytesOf(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::string hexOf(const keystore::Mac& mac) {
    return ECCrypto::bytesToHex(mac.data(), mac.size());
}

} // namespace

// Vectors are RFC 4231's, regenerated with Python's hashlib rather than
// transcribed, so they are an independent implementation's output.
TEST_CASE("hmacSha256 matches the RFC 4231 vectors", "[unit][keystore]") {
    SECTION("case 1: 20-byte key") {
        auto key = repeated(0x0b, 20);
        auto data = bytesOf("Hi There");
        REQUIRE(hexOf(keystore::hmacSha256(key.data(), key.size(),
                                           data.data(), data.size())) ==
                "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    }

    SECTION("case 2: short ASCII key") {
        auto key = bytesOf("Jefe");
        auto data = bytesOf("what do ya want for nothing?");
        REQUIRE(hexOf(keystore::hmacSha256(key.data(), key.size(),
                                           data.data(), data.size())) ==
                "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    }

    SECTION("case 3: 50 bytes of data") {
        auto key = repeated(0xaa, 20);
        auto data = repeated(0xdd, 50);
        REQUIRE(hexOf(keystore::hmacSha256(key.data(), key.size(),
                                           data.data(), data.size())) ==
                "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
    }

    SECTION("case 6: key longer than the block size is hashed first") {
        auto key = repeated(0xaa, 131);
        auto data = bytesOf("Test Using Larger Than Block-Size Key - Hash Key First");
        REQUIRE(hexOf(keystore::hmacSha256(key.data(), key.size(),
                                           data.data(), data.size())) ==
                "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    }
}

TEST_CASE("pbkdf2Sha256 matches the reference vectors", "[unit][keystore]") {
    auto salt = bytesOf("salt");

    SECTION("one iteration") {
        auto dk = keystore::pbkdf2Sha256("password", salt, 1, 32);
        REQUIRE(ECCrypto::bytesToHex(dk.data(), dk.size()) ==
                "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    }

    SECTION("two iterations") {
        auto dk = keystore::pbkdf2Sha256("password", salt, 2, 32);
        REQUIRE(ECCrypto::bytesToHex(dk.data(), dk.size()) ==
                "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    }

    SECTION("4096 iterations") {
        auto dk = keystore::pbkdf2Sha256("password", salt, 4096, 32);
        REQUIRE(ECCrypto::bytesToHex(dk.data(), dk.size()) ==
                "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    }

    SECTION("output longer than one hash block") {
        // Spans two PBKDF2 blocks, which is where an off-by-one in the block
        // index or the counter encoding shows up.
        auto long_salt = bytesOf("saltSALTsaltSALTsaltSALTsaltSALTsalt");
        auto dk = keystore::pbkdf2Sha256("passwordPASSWORDpassword",
                                         long_salt, 4096, 40);
        REQUIRE(ECCrypto::bytesToHex(dk.data(), dk.size()) ==
                "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1"
                "c635518c7dac47e9");
    }
}

TEST_CASE("encrypted blobs round-trip under the right password",
          "[unit][keystore]") {
    const std::string secret = "DEFAULT:abc\nabc:deadbeef:cafebabe\n";

    std::string armored = keystore::encrypt(secret, "correct horse", test_iterations);
    REQUIRE_FALSE(armored.empty());

    // The plaintext must not survive anywhere in the output.
    REQUIRE(armored.find("deadbeef") == std::string::npos);
    REQUIRE(armored.find("DEFAULT") == std::string::npos);

    auto recovered = keystore::decrypt(armored, "correct horse");
    REQUIRE(recovered.has_value());
    REQUIRE(*recovered == secret);
}

TEST_CASE("encryption is randomised per call", "[unit][keystore]") {
    // A fresh salt and nonce every time, so the same secret under the same
    // password never produces the same bytes twice.
    const std::string secret = "the same plaintext";
    REQUIRE(keystore::encrypt(secret, "pw", test_iterations) != keystore::encrypt(secret, "pw", test_iterations));
}

TEST_CASE("decrypt rejects a wrong password", "[unit][keystore]") {
    std::string armored = keystore::encrypt("private key material", "right", test_iterations);
    REQUIRE_FALSE(keystore::decrypt(armored, "wrong").has_value());
    REQUIRE_FALSE(keystore::decrypt(armored, "").has_value());
}

TEST_CASE("decrypt rejects tampered ciphertext", "[unit][keystore]") {
    // Encrypt-then-MAC: the tag covers the header and the ciphertext, so
    // flipping any of it must fail before anything is decrypted.
    std::string armored = keystore::encrypt("private key material", "pw", test_iterations);

    for (size_t offset : {size_t{0}, armored.size() / 2, armored.size() - 2}) {
        std::string tampered = armored;
        tampered[offset] = tampered[offset] == 'a' ? 'b' : 'a';
        INFO("flipped byte at offset " << offset);
        REQUIRE_FALSE(keystore::decrypt(tampered, "pw").has_value());
    }
}

TEST_CASE("decrypt rejects malformed input", "[unit][keystore]") {
    REQUIRE_FALSE(keystore::decrypt("", "pw").has_value());
    REQUIRE_FALSE(keystore::decrypt("not a keystore at all", "pw").has_value());
    REQUIRE_FALSE(keystore::decrypt("KEYSTORE1\nshort", "pw").has_value());
}
