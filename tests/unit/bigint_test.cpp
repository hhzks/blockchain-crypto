#include <catch2/catch_test_macros.hpp>
#include "bigint.h"

TEST_CASE("BigInt constructs from decimal and hex strings", "[unit][bigint]") {
    BigInt a("12345678901234567890");
    REQUIRE(a.to_string() == "12345678901234567890");
    BigInt b("FF", 16);
    REQUIRE(b.to_string() == "255");
    BigInt c("10", 16);
    REQUIRE(c.to_string() == "16");
}

TEST_CASE("BigInt arithmetic against known small values", "[unit][bigint]") {
    BigInt a(1000000000LL);
    BigInt b(1000000000LL);
    REQUIRE((a * b).to_string() == "1000000000000000000");
    REQUIRE((a + b).to_string() == "2000000000");
    REQUIRE((a - b).to_string() == "0");
    REQUIRE((a / BigInt(7LL)).to_string() == "142857142");
    REQUIRE((a % BigInt(7LL)).to_string() == "6");
}

TEST_CASE("BigInt handles negatives and zero", "[unit][bigint]") {
    BigInt zero("0");
    BigInt neg("-42");
    REQUIRE((neg + BigInt(42LL)) == zero);
    REQUIRE((-neg).to_string() == "42");
    REQUIRE(zero < BigInt(1LL));
    REQUIRE(neg < zero);
}

TEST_CASE("BigInt comparisons", "[unit][bigint]") {
    BigInt a("100");
    BigInt b("200");
    REQUIRE(a < b);
    REQUIRE(b > a);
    REQUIRE(a <= a);
    REQUIRE(a >= a);
    REQUIRE(a != b);
    REQUIRE(a == BigInt(100LL));
}

TEST_CASE("BigInt secp256k1 field size roundtrips through string", "[unit][bigint]") {
    BigInt p("115792089237316195423570985008687907853269984665640564039457584007908834671663");
    REQUIRE(p.to_string() ==
        "115792089237316195423570985008687907853269984665640564039457584007908834671663");
}
