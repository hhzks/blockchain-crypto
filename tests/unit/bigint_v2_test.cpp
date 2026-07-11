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
