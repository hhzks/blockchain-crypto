#include <catch2/catch_test_macros.hpp>
#include "sha.h"
#include "utils.h"
#include "vectors.h"

TEST_CASE("SHA256 passes NIST short vectors (empty, abc)", "[unit][sha]") {
    for (size_t i = 0; i < 2; ++i) {
        const auto& v = test_vectors::nist_sha256[i];
        INFO("input: " << v.input);
        REQUIRE(SHA256::hash(v.input) == v.expected_hex);
    }
}

TEST_CASE("SHA256 passes NIST 448-bit vector", "[unit][sha]") {
    // The 56-byte edge case exercises the two-block padding path (message
    // length mod 64 in [56, 63]) where an additional full block of padding
    // is required.
    const auto& v = test_vectors::nist_sha256[2];
    INFO("input: " << v.input);
    REQUIRE(SHA256::hash(v.input) == v.expected_hex);
}

TEST_CASE("SHA256 matches utils::sha256 (cross-consistency)", "[unit][sha]") {
    REQUIRE(SHA256::hash("the quick brown fox") == utils::sha256("the quick brown fox"));
}

TEST_CASE("SHA256 million 'a' matches NIST long vector", "[unit][sha]") {
    std::string input = test_vectors::million_a_input();
    REQUIRE(SHA256::hash(input) == test_vectors::million_a_expected);
}
