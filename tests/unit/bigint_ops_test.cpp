#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "bigint.h"

TEST_CASE("v2: long long construction and comparisons", "[unit][bigint]") {
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

TEST_CASE("v2: unary minus and zero canonicalization", "[unit][bigint]") {
    REQUIRE(-BigInt(5) == -5LL);
    REQUIRE(-BigInt(-5) == 5LL);
    REQUIRE(-BigInt(0) == 0LL);       // negative zero must not exist
    REQUIRE(+BigInt(-3) == -3LL);
}

TEST_CASE("v2: to_int in range and out of range", "[unit][bigint]") {
    REQUIRE(BigInt(0).to_int() == 0);
    REQUIRE(BigInt(15).to_int() == 15);
    REQUIRE(BigInt(-15).to_int() == -15);
    REQUIRE(BigInt(2147483647LL).to_int() == 2147483647);
    REQUIRE(BigInt(-2147483648LL).to_int() == -2147483647 - 1);
    REQUIRE_THROWS_AS(BigInt(2147483648LL).to_int(), std::out_of_range);
    REQUIRE_THROWS_AS(BigInt(-2147483649LL).to_int(), std::out_of_range);
}

TEST_CASE("v2: addition and subtraction, small values, all signs", "[unit][bigint]") {
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

TEST_CASE("v2: addition carries across the 64-bit limb boundary", "[unit][bigint]") {
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

TEST_CASE("v2: compound add/subtract assign", "[unit][bigint]") {
    BigInt x(10);
    x += BigInt(5);
    REQUIRE(x == 15LL);
    x -= BigInt(20);
    REQUIRE(x == -5LL);
}

TEST_CASE("v2: multiplication identities and signs", "[unit][bigint]") {
    REQUIRE(BigInt(6) * BigInt(7) == 42LL);
    REQUIRE(BigInt(-6) * BigInt(7) == -42LL);
    REQUIRE(BigInt(6) * BigInt(-7) == -42LL);
    REQUIRE(BigInt(-6) * BigInt(-7) == 42LL);
    REQUIRE(BigInt(6) * BigInt(0) == 0LL);
    REQUIRE(BigInt(0) * BigInt(-7) == 0LL);
    REQUIRE(BigInt(123) * 1LL == 123LL);
}

TEST_CASE("v2: multi-limb multiplication is consistent with addition", "[unit][bigint]") {
    // No string I/O exists yet, so verify algebraically against trusted ops.
    const long long max = 9223372036854775807LL;   // 2^63 - 1
    BigInt a(max);
    BigInt two_a = a + a;
    REQUIRE(a * BigInt(2) == two_a);               // 2^64 - 2: still one limb
    BigInt sq = a * a;                              // 126-bit result
    REQUIRE(sq + sq == a * two_a);                  // 2*a^2 == a*(2a)
    REQUIRE(sq > two_a);
    REQUIRE(a * a ==
            BigInt(std::string("85070591730234615847396907784232501249")));  // (2^63-1)^2
    BigInt x(3), acc;
    acc = sq + sq + sq;
    REQUIRE(sq * x == acc);                         // multiplication as repeated addition
    x *= BigInt(-5);
    REQUIRE(x == -15LL);
}

TEST_CASE("v2: decimal and hex string construction", "[unit][bigint]") {
    REQUIRE(BigInt(std::string("0")) == 0LL);
    REQUIRE(BigInt(std::string("42")) == 42LL);
    REQUIRE(BigInt(std::string("-42")) == -42LL);
    REQUIRE(BigInt(std::string("+42")) == 42LL);
    REQUIRE(BigInt(std::string("-0")) == 0LL);
    REQUIRE(BigInt(std::string("-0")).to_string() == "0");
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

TEST_CASE("v2: string construction rejects invalid input", "[unit][bigint]") {
    REQUIRE_THROWS_AS(BigInt(std::string("12a")), std::invalid_argument);
    REQUIRE_THROWS_AS(BigInt(std::string("")), std::invalid_argument);
    REQUIRE_THROWS_AS(BigInt(std::string("-")), std::invalid_argument);
    REQUIRE_THROWS_AS(BigInt("12", 1), std::invalid_argument);
    REQUIRE_THROWS_AS(BigInt("129", 8), std::invalid_argument);
}

TEST_CASE("v2: to_binary_string", "[unit][bigint]") {
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

TEST_CASE("v2: floored division and modulo across all sign combinations", "[unit][bigint]") {
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

TEST_CASE("v2: multi-limb division known answers and invariant", "[unit][bigint]") {
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

TEST_CASE("v2: to_string round-trips", "[unit][bigint]") {
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

TEST_CASE("v2: abs, squared, pow with legacy quirks", "[unit][bigint]") {
    REQUIRE(abs(BigInt(-9)) == 9LL);
    REQUIRE(abs(BigInt(9)) == 9LL);
    REQUIRE(squared(BigInt(-12)) == 144LL);
    REQUIRE(pow(BigInt(2), 10) == 1024LL);
    REQUIRE(pow(BigInt(-2), 3) == -8LL);
    REQUIRE(pow(BigInt(7), 1) == 7LL);
    REQUIRE(pow(BigInt(2), 128) ==
            BigInt(std::string("340282366920938463463374607431768211456")));
    // Legacy quirks preserved from the old header:
    REQUIRE(pow(BigInt(5), 0) == 1LL);
    REQUIRE(pow(BigInt(5), -2) == 0LL);
    REQUIRE(pow(BigInt(-1), -3) == -1LL);   // returns base when |base| == 1
    REQUIRE(pow(BigInt(1), -3) == 1LL);
    REQUIRE_THROWS_AS(pow(BigInt(0), -1), std::logic_error);
    REQUIRE_THROWS_AS(pow(BigInt(0), 0), std::logic_error);
}

TEST_CASE("v2: modular inverse against secp256k1 fixtures", "[unit][bigint]") {
    const BigInt P(std::string(
        "115792089237316195423570985008687907853269984665640564039457584007908834671663"));
    // inverse(a, P) * a = 1 (mod P), result normalized into [0, P)
    for (long long a : {2LL, 3LL, 65537LL}) {
        BigInt inv = inverse(BigInt(a), P);
        REQUIRE(inv > 0LL);
        REQUIRE(inv < P);
        REQUIRE((inv * BigInt(a)) % P == 1LL);
    }
    // Negative input is normalized first (the ECC point math passes
    // coordinate differences that can be negative).
    BigInt inv_neg = inverse(BigInt(-5), BigInt(17));
    REQUIRE(inv_neg == 10LL);                        // (-5 mod 17) = 12; 12*10 = 120 = 1 (mod 17)
    REQUIRE((inv_neg * (BigInt(-5) % BigInt(17))) % BigInt(17) == 1LL);
    // Large fixture: inverse of a 256-bit value
    BigInt gx(std::string(
        "55066263022277343669578718895168534326250603453777594175500187360389116729240"));
    BigInt inv_gx = inverse(gx, P);
    REQUIRE((inv_gx * gx) % P == 1LL);
}

TEST_CASE("floored division with multi-limb operands across sign combinations",
          "[unit][bigint]") {
    const BigInt P("fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f", 16);
    const BigInt a("123456789abcdef0fedcba9876543210deadbeefcafebabe0123456789abcdef", 16);
    const BigInt zero(0);

    // The ECC layer depends on (negative) % P landing in [0, P)
    BigInt r = (-a) % P;
    REQUIRE(r >= 0LL);
    REQUIRE(r < P);
    REQUIRE(r == P - (a % P));

    // All four sign combinations satisfy the floored invariant
    for (const BigInt& x : {a, -a}) {
        for (const BigInt& y : {P, -P}) {
            BigInt q = x / y;
            BigInt rem = x % y;
            REQUIRE(q * y + rem == x);
            if (y > zero) {
                REQUIRE(rem >= 0LL);
                REQUIRE(rem < y);
            } else {
                REQUIRE(rem <= 0LL);
                REQUIRE(rem > y);
            }
        }
    }

    // Exact multiples: no floored adjustment in either direction
    BigInt m = P * BigInt(-3);
    REQUIRE(m / P == -3LL);
    REQUIRE(m % P == 0LL);
    REQUIRE(m / (-P) == 3LL);
    REQUIRE(m % (-P) == 0LL);
}
