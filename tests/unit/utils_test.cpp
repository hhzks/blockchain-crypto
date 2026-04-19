#include <catch2/catch_test_macros.hpp>
#include "utils.h"
#include "vectors.h"

TEST_CASE("sha256 output is 64-char lowercase hex", "[unit][utils]") {
    std::string h = utils::sha256("hello");
    REQUIRE(h.size() == 64);
    for (char c : h) {
        REQUIRE(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
    }
}

TEST_CASE("sha256 empty-string matches known digest", "[unit][utils]") {
    REQUIRE(utils::sha256("") == test_vectors::nist_sha256[0].expected_hex);
}

TEST_CASE("sha256 is deterministic", "[unit][utils]") {
    REQUIRE(utils::sha256("xyz") == utils::sha256("xyz"));
}

TEST_CASE("calculateMerkleRoot handles 0, 1, 2, 3, 4 tx cases", "[unit][utils]") {
    REQUIRE(utils::calculateMerkleRoot({}) == utils::sha256(""));
    REQUIRE(utils::calculateMerkleRoot({"deadbeef"}) == "deadbeef");
    std::string two = utils::calculateMerkleRoot({"a", "b"});
    REQUIRE(two == utils::sha256("ab"));
    std::string three = utils::calculateMerkleRoot({"a", "b", "c"});
    std::string ab = utils::sha256("ab");
    std::string cc = utils::sha256(std::string("c") + "c");
    REQUIRE(three == utils::sha256(ab + cc));
    std::string four = utils::calculateMerkleRoot({"a", "b", "c", "d"});
    REQUIRE(four == utils::sha256(utils::sha256("ab") + utils::sha256("cd")));
}

TEST_CASE("checkProofOfWork boundary at each difficulty", "[unit][utils]") {
    REQUIRE(utils::checkProofOfWork("0000abc", 4));
    REQUIRE_FALSE(utils::checkProofOfWork("0001abc", 4));
    REQUIRE(utils::checkProofOfWork("0abc", 1));
    REQUIRE(utils::checkProofOfWork("any_string", 0));
}

TEST_CASE("bytesToHex produces expected hex", "[unit][utils]") {
    std::vector<unsigned char> data = {0x00, 0xff, 0xab, 0x10};
    REQUIRE(utils::bytesToHex(data) == "00ffab10");
}
