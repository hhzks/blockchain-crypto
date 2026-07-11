#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "bigint_v2.h"

using bigint2::BigInt;

TEST_CASE("v2: long long construction and comparisons", "[unit][bigint2]") {
    REQUIRE(BigInt(0) == BigInt(0));
    REQUIRE(BigInt(42) == 42LL);
    REQUIRE(BigInt(-42) == -42LL);
    REQUIRE(BigInt(-42) != 42LL);
    REQUIRE(BigInt(-1) < BigInt(0));
    REQUIRE(BigInt(0) < BigInt(1));
    REQUIRE(BigInt(-100) < BigInt(-1));
    REQUIRE(BigInt(7) > 6LL);
    REQUIRE(BigInt(7) >= 7LL);
    REQUIRE(BigInt(7) <= 7LL);
    REQUIRE(BigInt(9223372036854775807LL) == 9223372036854775807LL);
    // LLONG_MIN magnitude does not fit in a positive long long — must not overflow
    REQUIRE(BigInt(-9223372036854775807LL - 1) < BigInt(-9223372036854775807LL));
}

TEST_CASE("v2: unary minus and zero canonicalization", "[unit][bigint2]") {
    REQUIRE(-BigInt(5) == -5LL);
    REQUIRE(-BigInt(-5) == 5LL);
    REQUIRE(-BigInt(0) == 0LL);       // negative zero must not exist
    REQUIRE(+BigInt(-3) == -3LL);
}

TEST_CASE("v2: to_int in range and out of range", "[unit][bigint2]") {
    REQUIRE(BigInt(0).to_int() == 0);
    REQUIRE(BigInt(15).to_int() == 15);
    REQUIRE(BigInt(-15).to_int() == -15);
    REQUIRE(BigInt(2147483647LL).to_int() == 2147483647);
    REQUIRE(BigInt(-2147483648LL).to_int() == -2147483647 - 1);
    REQUIRE_THROWS_AS(BigInt(2147483648LL).to_int(), std::out_of_range);
    REQUIRE_THROWS_AS(BigInt(-2147483649LL).to_int(), std::out_of_range);
}

TEST_CASE("v2: addition and subtraction, small values, all signs", "[unit][bigint2]") {
    REQUIRE(BigInt(2) + BigInt(3) == 5LL);
    REQUIRE(BigInt(-2) + BigInt(-3) == -5LL);
    REQUIRE(BigInt(5) + BigInt(-3) == 2LL);
    REQUIRE(BigInt(3) + BigInt(-5) == -2LL);
    REQUIRE(BigInt(5) - BigInt(3) == 2LL);
    REQUIRE(BigInt(3) - BigInt(5) == -2LL);
    REQUIRE(BigInt(-3) - BigInt(-5) == 2LL);
    REQUIRE(BigInt(7) + BigInt(-7) == 0LL);
    REQUIRE(BigInt(7) - BigInt(7) == 0LL);
}

TEST_CASE("v2: addition carries across the 64-bit limb boundary", "[unit][bigint2]") {
    const long long max = 9223372036854775807LL;   // 2^63 - 1
    BigInt sum = BigInt(max) + BigInt(max);        // 2^64 - 2: still one limb
    REQUIRE(sum > BigInt(max));
    REQUIRE(sum - BigInt(max) == max);
    // max + max + 2 = 2^64 exactly: carry-out forces a second limb
    BigInt two_limbs = sum + BigInt(2);
    REQUIRE(two_limbs > sum);
    // Borrow back across the boundary and normalize away the high limb
    REQUIRE(two_limbs - BigInt(1) > sum);           // 2^64 - 1: one limb again
    REQUIRE(two_limbs - BigInt(2) == sum);          // full round trip
    // Mismatched limb counts (two-limb minus one-limb) and down to zero
    BigInt back = two_limbs - BigInt(max) - BigInt(max) - BigInt(2);
    REQUIRE(back == 0LL);
    REQUIRE(two_limbs + BigInt(max) - BigInt(max) == two_limbs);
}

TEST_CASE("v2: compound add/subtract assign", "[unit][bigint2]") {
    BigInt x(10);
    x += BigInt(5);
    REQUIRE(x == 15LL);
    x -= BigInt(20);
    REQUIRE(x == -5LL);
}

TEST_CASE("v2: multiplication identities and signs", "[unit][bigint2]") {
    REQUIRE(BigInt(6) * BigInt(7) == 42LL);
    REQUIRE(BigInt(-6) * BigInt(7) == -42LL);
    REQUIRE(BigInt(6) * BigInt(-7) == -42LL);
    REQUIRE(BigInt(-6) * BigInt(-7) == 42LL);
    REQUIRE(BigInt(6) * BigInt(0) == 0LL);
    REQUIRE(BigInt(0) * BigInt(-7) == 0LL);
    REQUIRE(BigInt(123) * 1LL == 123LL);
}

TEST_CASE("v2: multi-limb multiplication is consistent with addition", "[unit][bigint2]") {
    // No string I/O exists yet, so verify algebraically against trusted ops.
    const long long max = 9223372036854775807LL;   // 2^63 - 1
    BigInt a(max);
    BigInt two_a = a + a;
    REQUIRE(a * BigInt(2) == two_a);               // crosses into limb 2
    BigInt sq = a * a;                              // 126-bit result
    REQUIRE(sq + sq == a * two_a);                  // 2*a^2 == a*(2a)
    REQUIRE(sq > two_a);
    REQUIRE(a * a == a * a);                        // deterministic
    BigInt x(3), acc;
    acc = sq + sq + sq;
    REQUIRE(sq * x == acc);                         // multiplication as repeated addition
    x *= BigInt(-5);
    REQUIRE(x == -15LL);
}

TEST_CASE("v2: decimal and hex string construction", "[unit][bigint2]") {
    REQUIRE(BigInt(std::string("0")) == 0LL);
    REQUIRE(BigInt(std::string("42")) == 42LL);
    REQUIRE(BigInt(std::string("-42")) == -42LL);
    REQUIRE(BigInt(std::string("+42")) == 42LL);
    REQUIRE(BigInt("ff", 16) == 255LL);
    REQUIRE(BigInt("FF", 16) == 255LL);
    REQUIRE(BigInt("-ff", 16) == -255LL);
    REQUIRE(BigInt("101", 2) == 5LL);
    REQUIRE(BigInt("zz", 36) == 35LL * 36 + 35);
    // Known cross-base answer: secp256k1's field prime
    BigInt p_dec(std::string(
        "115792089237316195423570985008687907853269984665640564039457584007908834671663"));
    BigInt p_hex(
        "fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f", 16);
    REQUIRE(p_dec == p_hex);
    // Exact known product: (2^63 - 1)^2
    BigInt max(9223372036854775807LL);
    REQUIRE(max * max ==
            BigInt(std::string("85070591730234615847396907784232501249")));
}

TEST_CASE("v2: string construction rejects invalid input", "[unit][bigint2]") {
    REQUIRE_THROWS_AS(BigInt(std::string("12a")), std::invalid_argument);
    REQUIRE_THROWS_AS(BigInt(std::string("")), std::invalid_argument);
    REQUIRE_THROWS_AS(BigInt(std::string("-")), std::invalid_argument);
    REQUIRE_THROWS_AS(BigInt("12", 1), std::invalid_argument);
    REQUIRE_THROWS_AS(BigInt("129", 8), std::invalid_argument);
}

TEST_CASE("v2: to_binary_string", "[unit][bigint2]") {
    REQUIRE(BigInt(0).to_binary_string() == "0");
    REQUIRE(BigInt(10).to_binary_string() == "1010");
    REQUIRE(BigInt(-5).to_binary_string() == "-101");
    // 2^64 = "1" followed by 64 zeros
    std::string expected = "1" + std::string(64, '0');
    REQUIRE(BigInt("10000000000000000", 16).to_binary_string() == expected);
    // Round-trip through base-2 construction
    BigInt p("fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f", 16);
    REQUIRE(BigInt(p.to_binary_string(), 2) == p);
}

TEST_CASE("v2: floored division and modulo across all sign combinations", "[unit][bigint2]") {
    // Floored: quotient rounds toward negative infinity, remainder has
    // the divisor's sign. (Python semantics; the ECC layer depends on it.)
    REQUIRE(BigInt(7)  / BigInt(3)  == 2LL);
    REQUIRE(BigInt(7)  % BigInt(3)  == 1LL);
    REQUIRE(BigInt(-7) / BigInt(3)  == -3LL);
    REQUIRE(BigInt(-7) % BigInt(3)  == 2LL);
    REQUIRE(BigInt(7)  / BigInt(-3) == -3LL);
    REQUIRE(BigInt(7)  % BigInt(-3) == -2LL);
    REQUIRE(BigInt(-7) / BigInt(-3) == 2LL);
    REQUIRE(BigInt(-7) % BigInt(-3) == -1LL);
    // Exact multiples: no floored adjustment
    REQUIRE(BigInt(-6) / BigInt(3) == -2LL);
    REQUIRE(BigInt(-6) % BigInt(3) == 0LL);
    // |dividend| < |divisor|
    REQUIRE(BigInt(2) / BigInt(5) == 0LL);
    REQUIRE(BigInt(-2) / BigInt(5) == -1LL);
    REQUIRE(BigInt(-2) % BigInt(5) == 3LL);
    // long long overloads
    REQUIRE(BigInt(-7) % 3LL == 2LL);
    REQUIRE(BigInt(255) / 16LL == 15LL);
    // Invariant a == (a/b)*b + a%b on mixed signs
    BigInt a(-97), b(13);
    REQUIRE((a / b) * b + (a % b) == a);
    REQUIRE_THROWS_AS(BigInt(1) / BigInt(0), std::logic_error);
    REQUIRE_THROWS_AS(BigInt(1) % BigInt(0), std::logic_error);
}

TEST_CASE("v2: multi-limb division known answers and invariant", "[unit][bigint2]") {
    // (2^128 - 1) = (2^64 + 1)(2^64 - 1) exactly
    BigInt a("ffffffffffffffffffffffffffffffff", 16);
    BigInt b("10000000000000001", 16);
    REQUIRE(a / b == BigInt("ffffffffffffffff", 16));
    REQUIRE(a % b == 0LL);
    // Adversarial: minimal normalized divisor, near-maximal quotient.
    // Construct dividend = q*d + r from parts, then require exact recovery.
    BigInt d("8000000000000000ffffffffffffffff", 16);
    BigInt q("ffffffffffffffffffffffffffffffff", 16);
    BigInt r = d - BigInt(1);
    BigInt dividend = q * d + r;
    REQUIRE(dividend / d == q);
    REQUIRE(dividend % d == r);
    // Pseudo-random breadth: deterministic LCG, invariant must hold.
    uint64_t seed = 0x2545F4914F6CDD1DULL;
    auto next = [&seed]() {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        return seed;
    };
    for (int i = 0; i < 200; ++i) {
        BigInt x = (BigInt(static_cast<long long>(next() >> 1)) * BigInt(static_cast<long long>(next() >> 1)) +
                    BigInt(static_cast<long long>(next() >> 1))) * BigInt(static_cast<long long>(next() >> 1));
        BigInt y = BigInt(static_cast<long long>(next() >> 1)) * BigInt(static_cast<long long>(next() >> 1)) + BigInt(1);
        BigInt quot = x / y;
        BigInt rem = x % y;
        REQUIRE(quot * y + rem == x);
        REQUIRE(rem >= 0LL);
        REQUIRE(rem < y);
    }
}

TEST_CASE("v2: to_string round-trips", "[unit][bigint2]") {
    REQUIRE(BigInt(0).to_string() == "0");
    REQUIRE(BigInt(42).to_string() == "42");
    REQUIRE(BigInt(-42).to_string() == "-42");
    REQUIRE(BigInt(10000000000000000000ULL % 9223372036854775807LL).to_string()
            == "776627963145224193");
    const std::string p_dec =
        "115792089237316195423570985008687907853269984665640564039457584007908834671663";
    REQUIRE(BigInt(p_dec).to_string() == p_dec);
    REQUIRE(BigInt(std::string("-") + p_dec).to_string() == "-" + p_dec);
    // Interior zero chunks must be zero-padded
    REQUIRE(BigInt("100000000000000000000", 10).to_string() == "100000000000000000000");
}
