#pragma once
#include <string>
#include <array>
#include <cstdint>

namespace test_vectors {

// NIST FIPS-180-4 SHA-256 test vectors (lowercase hex)
struct Sha256Vector { const char* input; const char* expected_hex; };

inline constexpr Sha256Vector nist_sha256[] = {
    {"",
     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {"abc",
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
    {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
};

inline std::string million_a_input() { return std::string(1'000'000, 'a'); }
inline constexpr const char* million_a_expected =
    "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";

// secp256k1 generator point (priv=1 yields public key == G)
inline constexpr const char* secp256k1_G_x =
    "79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798";
inline constexpr const char* secp256k1_G_y =
    "483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8";

// Deterministic fixture keypair (priv = 0x0000...001234)
inline constexpr const char* fixture_priv_hex =
    "0000000000000000000000000000000000000000000000000000000000001234";

} // namespace test_vectors
