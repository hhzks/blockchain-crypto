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

// Issue #14: a negative difficulty reached std::string(count, char) as a huge
// size_t and threw std::length_error; a huge positive one allocated gigabytes.
TEST_CASE("checkProofOfWork rejects out-of-range difficulty", "[unit][utils]") {
    const std::string zeros(64, '0');

    REQUIRE_FALSE(utils::checkProofOfWork(zeros, -1));
    REQUIRE_FALSE(utils::checkProofOfWork(zeros, -2000000000));

    // A hash is 64 hex characters, so nothing beyond that can ever be satisfied.
    REQUIRE_FALSE(utils::checkProofOfWork(zeros, 65));
    REQUIRE_FALSE(utils::checkProofOfWork(zeros, 2000000000));

    // The boundary itself stays satisfiable.
    REQUIRE(utils::checkProofOfWork(zeros, 64));
}

// Issue #9: handleBlocks divided by a peer-supplied block count.
TEST_CASE("percentComplete never divides by a non-positive total", "[unit][utils]") {
    REQUIRE(utils::percentComplete(1, 0) == 0);
    REQUIRE(utils::percentComplete(1, -1) == 0);
    REQUIRE(utils::percentComplete(0, 0) == 0);
}

TEST_CASE("percentComplete reports bounded progress", "[unit][utils]") {
    REQUIRE(utils::percentComplete(0, 10) == 0);
    REQUIRE(utils::percentComplete(5, 10) == 50);
    REQUIRE(utils::percentComplete(10, 10) == 100);
    // A peer can claim fewer blocks than it sends; progress must still be sane.
    REQUIRE(utils::percentComplete(20, 10) == 100);
    REQUIRE(utils::percentComplete(-5, 10) == 0);
}

// Issue #23: std::cin >> int value-initialises to 0 on failure, and 0 is the
// menu's Exit case, so any junk input quit the program.
TEST_CASE("parseInt accepts a complete integer with surrounding space", "[unit][utils]") {
    REQUIRE(utils::parseInt("3") == 3);
    REQUIRE(utils::parseInt("  4  ") == 4);
    REQUIRE(utils::parseInt("-7") == -7);
    REQUIRE(utils::parseInt("0") == 0);
    // Windows line endings survive getline on a text stream.
    REQUIRE(utils::parseInt("5\r") == 5);
}

TEST_CASE("parseInt rejects anything that is not a whole integer", "[unit][utils]") {
    REQUIRE_FALSE(utils::parseInt("").has_value());
    REQUIRE_FALSE(utils::parseInt("   ").has_value());
    REQUIRE_FALSE(utils::parseInt("q").has_value());
    REQUIRE_FALSE(utils::parseInt("1x").has_value());
    REQUIRE_FALSE(utils::parseInt("1 2").has_value());
    REQUIRE_FALSE(utils::parseInt("0x10").has_value());
    REQUIRE_FALSE(utils::parseInt("3.5").has_value());
    // Out of int range must not silently wrap.
    REQUIRE_FALSE(utils::parseInt("99999999999999999999").has_value());
}

TEST_CASE("parseDouble accepts finite decimal values", "[unit][utils]") {
    REQUIRE(utils::parseDouble("2.5") == 2.5);
    REQUIRE(utils::parseDouble("  10  ") == 10.0);
    REQUIRE(utils::parseDouble("-3") == -3.0);
    REQUIRE(utils::parseDouble("0") == 0.0);
}

TEST_CASE("parseDouble rejects junk and non-finite values", "[unit][utils]") {
    REQUIRE_FALSE(utils::parseDouble("").has_value());
    REQUIRE_FALSE(utils::parseDouble("abc").has_value());
    REQUIRE_FALSE(utils::parseDouble("1.5x").has_value());
    // from_chars parses these happily; a currency amount must not be either.
    REQUIRE_FALSE(utils::parseDouble("nan").has_value());
    REQUIRE_FALSE(utils::parseDouble("inf").has_value());
    REQUIRE_FALSE(utils::parseDouble("-inf").has_value());
}
