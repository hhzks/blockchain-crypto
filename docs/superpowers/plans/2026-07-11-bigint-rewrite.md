# BigInt Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `src/include/bigint.h` (modified faheel/BigInt, decimal-string based) with an original 64-bit-limb implementation that is a strict drop-in and orders of magnitude faster.

**Architecture:** The new class is developed in a parallel header `src/include/bigint_v2.h` inside `namespace bigint2`, with its own Catch2 test file, so the existing suite stays green while it grows (the old `bigint.h` cannot be replaced piecemeal — `ECCrypto.h` constructs `BigInt` constants at static initialization, so a partial header aborts every test binary at startup). The final task strips the namespace, moves the code into `bigint.h`, deletes the scaffolding, and records before/after suite timing.

**Tech Stack:** C++23, GCC (w64devkit, pinned in CMakeLists) with `unsigned __int128`, Catch2 v3 via CMake `BUILD_TESTS=ON`, MinGW Makefiles generator (build dir already configured at `build/`).

**Spec:** `docs/superpowers/specs/2026-07-11-bigint-rewrite-design.md`

## Global Constraints

- Strict drop-in: `ECCrypto.cpp`, `ECCrypto.h`, and all existing tests compile unchanged after the final swap.
- **Floored division semantics**: `a / b` rounds toward negative infinity; `a % b` has the divisor's sign (or is zero); `a == (a/b)*b + (a%b)` always. Division/modulo by zero throws `std::logic_error`.
- Constructors must NOT be `explicit` — callers rely on implicit conversion (`BigInt k = 0;`, `ECPoint{0, 0, p, a, b}`).
- Representation: sign-magnitude, `std::vector<uint64_t>` limbs little-endian, canonical form (no high zero limbs; zero = empty vector, never negative).
- Original code only: implement from the algorithm descriptions in this plan; do not open the old `bigint.h`, `archive/bigint.*`, or upstream faheel/BigInt.
- Commit messages: never mention Claude, no Co-Authored-By/attribution lines.
- Build commands (from repo root `C:\Users\H\Documents\blockchain`):
  - Rebuild tests: `cmake --build build --target blockchain_tests -j 8`
  - Run new-class tests: `build\tests\blockchain_tests.exe "[bigint2]"`
  - Run everything: `build\tests\blockchain_tests.exe`
- Work on a branch: `git checkout -b bigint-rewrite` before Task 1 (or the worktree the executing skill provides).

## File Structure

- Create (Tasks 1–6): `src/include/bigint_v2.h` — the complete new implementation, header-only, `namespace bigint2`.
- Create (Task 1), grow (Tasks 2–6): `tests/unit/bigint_v2_test.cpp` — tests tagged `[bigint2]`.
- Modify (Task 1): `tests/CMakeLists.txt` — add the new test file.
- Task 7 swap: `src/include/bigint.h` replaced with the v2 code minus namespace; `bigint_v2.h` deleted; test file renamed to `tests/unit/bigint_ops_test.cpp` with includes/tags updated; `tests/CMakeLists.txt` updated. `tests/unit/bigint_test.cpp` (existing 5 cases) is untouched and must pass against the new implementation.

---

### Task 1: Skeleton — representation, long long construction, comparisons, to_int

**Files:**
- Create: `src/include/bigint_v2.h`
- Create: `tests/unit/bigint_v2_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `bigint2::BigInt` with `BigInt()`, `BigInt(const long long&)`, copy/assign, unary `+`/`-`, all six comparisons vs `BigInt` and vs `long long`, `to_int()`. Private helpers `normalize()`, `isZero()`, `compareMagnitude(a, b)`, `compare(other)` that every later task builds on.

- [ ] **Step 1: Add the test file to the build and write failing tests**

In `tests/CMakeLists.txt`, add to `TEST_SOURCES` after `unit/bigint_test.cpp`:

```cmake
    unit/bigint_v2_test.cpp
```

Create `tests/unit/bigint_v2_test.cpp`:

```cpp
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
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build --target blockchain_tests -j 8`
Expected: FAIL to compile — `bigint_v2.h: No such file or directory`.

- [ ] **Step 3: Create the header skeleton**

Create `src/include/bigint_v2.h`:

```cpp
#pragma once

#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace bigint2 {

// Arbitrary-precision signed integer.
// Sign-magnitude representation: `limbs` is the little-endian base-2^64
// magnitude (limbs[0] least significant). Canonical form: no high zero
// limbs; zero is the empty vector and is never negative.
// Division semantics are floored: a/b rounds toward negative infinity and
// a%b carries the divisor's sign, so a == (a/b)*b + (a%b) always holds and
// a % p is in [0, p) for positive p — the ECC layer depends on this.
class BigInt {
private:
    std::vector<uint64_t> limbs;
    bool negative = false;

    void normalize() {
        while (!limbs.empty() && limbs.back() == 0) limbs.pop_back();
        if (limbs.empty()) negative = false;
    }

    bool isZero() const { return limbs.empty(); }

    // -1, 0, or 1 as |a| <, ==, > |b|
    static int compareMagnitude(const std::vector<uint64_t>& a,
                                const std::vector<uint64_t>& b) {
        if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
        for (size_t i = a.size(); i-- > 0;) {
            if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        }
        return 0;
    }

    // -1, 0, or 1 as *this <, ==, > other (signed)
    int compare(const BigInt& other) const {
        if (negative != other.negative) return negative ? -1 : 1;
        int mag = compareMagnitude(limbs, other.limbs);
        return negative ? -mag : mag;
    }

public:
    BigInt() = default;
    BigInt(const BigInt&) = default;
    BigInt& operator=(const BigInt&) = default;

    BigInt(const long long& num) {
        negative = num < 0;
        // 0ULL - x computes |x| without overflowing on LLONG_MIN.
        unsigned long long magnitude =
            negative ? 0ULL - static_cast<unsigned long long>(num)
                     : static_cast<unsigned long long>(num);
        if (magnitude != 0) limbs.push_back(magnitude);
    }

    BigInt& operator=(const long long& num) { return *this = BigInt(num); }

    BigInt operator+() const { return *this; }

    BigInt operator-() const {
        BigInt result = *this;
        if (!result.isZero()) result.negative = !result.negative;
        return result;
    }

    bool operator==(const BigInt& o) const { return compare(o) == 0; }
    bool operator!=(const BigInt& o) const { return compare(o) != 0; }
    bool operator<(const BigInt& o) const  { return compare(o) < 0; }
    bool operator>(const BigInt& o) const  { return compare(o) > 0; }
    bool operator<=(const BigInt& o) const { return compare(o) <= 0; }
    bool operator>=(const BigInt& o) const { return compare(o) >= 0; }

    bool operator==(const long long& n) const { return *this == BigInt(n); }
    bool operator!=(const long long& n) const { return *this != BigInt(n); }
    bool operator<(const long long& n) const  { return *this < BigInt(n); }
    bool operator>(const long long& n) const  { return *this > BigInt(n); }
    bool operator<=(const long long& n) const { return *this <= BigInt(n); }
    bool operator>=(const long long& n) const { return *this >= BigInt(n); }

    int to_int() const {
        constexpr uint64_t int_max =
            static_cast<uint64_t>(std::numeric_limits<int>::max());
        if (limbs.size() > 1)
            throw std::out_of_range("BigInt::to_int: value out of int range");
        uint64_t v = isZero() ? 0 : limbs[0];
        if (v > int_max + (negative ? 1u : 0u))
            throw std::out_of_range("BigInt::to_int: value out of int range");
        return negative ? static_cast<int>(-static_cast<int64_t>(v))
                        : static_cast<int>(v);
    }
};

} // namespace bigint2
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build build --target blockchain_tests -j 8`, then `build\tests\blockchain_tests.exe "[bigint2]"`
Expected: `All tests passed` (3 test cases). Also run `build\tests\blockchain_tests.exe "[bigint]"` — the old-class tests still pass (old header untouched).

- [ ] **Step 5: Commit**

```bash
git add src/include/bigint_v2.h tests/unit/bigint_v2_test.cpp tests/CMakeLists.txt
git commit -m "feat: bigint v2 skeleton - limbs, comparisons, to_int"
```

---

### Task 2: Addition and subtraction

**Files:**
- Modify: `src/include/bigint_v2.h`
- Modify: `tests/unit/bigint_v2_test.cpp`

**Interfaces:**
- Consumes: `compareMagnitude`, `normalize`, sign flags from Task 1.
- Produces: private `addMagnitude(a, b)`, `subMagnitude(a, b)` (requires `|a| >= |b|`); public `operator+`, `operator-`, `operator+=`, `operator-=` (all `const BigInt&`).

- [ ] **Step 1: Write failing tests** (append to `tests/unit/bigint_v2_test.cpp`)

```cpp
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
    BigInt sum = BigInt(max) + BigInt(max);        // 2^64 - 2: needs two limbs
    REQUIRE(sum > BigInt(max));
    REQUIRE(sum - BigInt(max) == max);             // round-trips through borrow
    BigInt back = sum - BigInt(max) - BigInt(max);
    REQUIRE(back == 0LL);
}

TEST_CASE("v2: compound add/subtract assign", "[unit][bigint2]") {
    BigInt x(10);
    x += BigInt(5);
    REQUIRE(x == 15LL);
    x -= BigInt(20);
    REQUIRE(x == -5LL);
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build --target blockchain_tests -j 8`
Expected: FAIL to compile — `no match for 'operator+'`.

- [ ] **Step 3: Implement**

Add to the private section of `BigInt` in `src/include/bigint_v2.h` (after `compare`):

```cpp
    static std::vector<uint64_t> addMagnitude(const std::vector<uint64_t>& a,
                                              const std::vector<uint64_t>& b) {
        const size_t n = a.size() > b.size() ? a.size() : b.size();
        std::vector<uint64_t> result;
        result.reserve(n + 1);
        unsigned __int128 carry = 0;
        for (size_t i = 0; i < n; ++i) {
            unsigned __int128 sum = carry;
            if (i < a.size()) sum += a[i];
            if (i < b.size()) sum += b[i];
            result.push_back(static_cast<uint64_t>(sum));
            carry = sum >> 64;
        }
        if (carry != 0) result.push_back(static_cast<uint64_t>(carry));
        return result;
    }

    // Requires |a| >= |b|.
    static std::vector<uint64_t> subMagnitude(const std::vector<uint64_t>& a,
                                              const std::vector<uint64_t>& b) {
        std::vector<uint64_t> result(a.size());
        __int128 borrow = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            __int128 diff = static_cast<__int128>(a[i]) - borrow -
                            (i < b.size() ? b[i] : 0);
            if (diff < 0) {
                diff += static_cast<__int128>(1) << 64;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result[i] = static_cast<uint64_t>(diff);
        }
        return result;
    }
```

Add to the public section:

```cpp
    BigInt operator+(const BigInt& num) const {
        BigInt result;
        if (negative == num.negative) {
            result.limbs = addMagnitude(limbs, num.limbs);
            result.negative = negative;
        } else {
            int cmp = compareMagnitude(limbs, num.limbs);
            if (cmp == 0) return BigInt();
            if (cmp > 0) {
                result.limbs = subMagnitude(limbs, num.limbs);
                result.negative = negative;
            } else {
                result.limbs = subMagnitude(num.limbs, limbs);
                result.negative = num.negative;
            }
        }
        result.normalize();
        return result;
    }

    BigInt operator-(const BigInt& num) const { return *this + (-num); }

    BigInt& operator+=(const BigInt& num) { return *this = *this + num; }
    BigInt& operator-=(const BigInt& num) { return *this = *this - num; }
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build build --target blockchain_tests -j 8`, then `build\tests\blockchain_tests.exe "[bigint2]"`
Expected: `All tests passed` (6 test cases).

- [ ] **Step 5: Commit**

```bash
git add src/include/bigint_v2.h tests/unit/bigint_v2_test.cpp
git commit -m "feat: bigint v2 addition and subtraction"
```

---

### Task 3: Multiplication

**Files:**
- Modify: `src/include/bigint_v2.h`
- Modify: `tests/unit/bigint_v2_test.cpp`

**Interfaces:**
- Consumes: add/sub from Task 2 (tests verify products against sums).
- Produces: private `mulMagnitude(a, b)`; public `operator*(const BigInt&)`, `operator*(const long long&)`, `operator*=(const BigInt&)`.

- [ ] **Step 1: Write failing tests** (append)

```cpp
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
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build --target blockchain_tests -j 8`
Expected: FAIL to compile — `no match for 'operator*'`.

- [ ] **Step 3: Implement**

Add to the private section:

```cpp
    static std::vector<uint64_t> mulMagnitude(const std::vector<uint64_t>& a,
                                              const std::vector<uint64_t>& b) {
        if (a.empty() || b.empty()) return {};
        std::vector<uint64_t> result(a.size() + b.size(), 0);
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned __int128 carry = 0;
            for (size_t j = 0; j < b.size(); ++j) {
                unsigned __int128 cur =
                    static_cast<unsigned __int128>(a[i]) * b[j] +
                    result[i + j] + carry;
                result[i + j] = static_cast<uint64_t>(cur);
                carry = cur >> 64;
            }
            for (size_t k = i + b.size(); carry != 0; ++k) {
                unsigned __int128 cur = result[k] + carry;
                result[k] = static_cast<uint64_t>(cur);
                carry = cur >> 64;
            }
        }
        return result;
    }
```

Add to the public section:

```cpp
    BigInt operator*(const BigInt& num) const {
        BigInt result;
        result.limbs = mulMagnitude(limbs, num.limbs);
        result.negative = (negative != num.negative) && !result.limbs.empty();
        result.normalize();
        return result;
    }

    BigInt operator*(const long long& num) const { return *this * BigInt(num); }
    BigInt& operator*=(const BigInt& num) { return *this = *this * num; }
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build build --target blockchain_tests -j 8`, then `build\tests\blockchain_tests.exe "[bigint2]"`
Expected: `All tests passed` (8 test cases).

- [ ] **Step 5: Commit**

```bash
git add src/include/bigint_v2.h tests/unit/bigint_v2_test.cpp
git commit -m "feat: bigint v2 schoolbook multiplication"
```

---

### Task 4: String construction (decimal + base 2–36) and to_binary_string

**Files:**
- Modify: `src/include/bigint_v2.h`
- Modify: `tests/unit/bigint_v2_test.cpp`

**Interfaces:**
- Consumes: `mulMagnitude` conceptually (parsing uses a dedicated in-place helper), add from Task 2 for tests.
- Produces: private `mulAddInPlace(multiplier, addend)`, `digitValue(char)`; public `BigInt(const std::string&)`, `BigInt(const std::string&, int base)`, `to_binary_string()`.

- [ ] **Step 1: Write failing tests** (append)

```cpp
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
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build --target blockchain_tests -j 8`
Expected: FAIL to compile — no matching constructor for `BigInt(std::string)`.

- [ ] **Step 3: Implement**

Add to the private section:

```cpp
    // *this = *this * multiplier + addend, on the magnitude. Sign untouched.
    void mulAddInPlace(uint64_t multiplier, uint64_t addend) {
        unsigned __int128 carry = addend;
        for (uint64_t& limb : limbs) {
            unsigned __int128 cur =
                static_cast<unsigned __int128>(limb) * multiplier + carry;
            limb = static_cast<uint64_t>(cur);
            carry = cur >> 64;
        }
        while (carry != 0) {
            limbs.push_back(static_cast<uint64_t>(carry));
            carry >>= 64;
        }
    }

    static int digitValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'z') return c - 'a' + 10;
        if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
        return -1;
    }
```

Add to the public section (after the `long long` assignment operator):

```cpp
    BigInt(const std::string& num) : BigInt(num, 10) {}

    BigInt(const std::string& num, int base) {
        if (base < 2 || base > 36)
            throw std::invalid_argument("BigInt: base must be between 2 and 36");
        size_t pos = 0;
        bool neg = false;
        if (pos < num.size() && (num[pos] == '+' || num[pos] == '-')) {
            neg = (num[pos] == '-');
            ++pos;
        }
        if (pos == num.size())
            throw std::invalid_argument("BigInt: expected an integer, got: " + num);

        // Largest digit count whose chunk value always fits in uint64_t.
        const uint64_t ubase = static_cast<uint64_t>(base);
        uint64_t chunk_cap = 1;
        size_t chunk_len = 0;
        while (chunk_cap <= std::numeric_limits<uint64_t>::max() / ubase) {
            chunk_cap *= ubase;
            ++chunk_len;
        }

        size_t take = (num.size() - pos) % chunk_len;
        if (take == 0) take = chunk_len;
        while (pos < num.size()) {
            uint64_t chunk_value = 0;
            uint64_t chunk_mult = 1;
            for (size_t i = 0; i < take; ++i, ++pos) {
                int d = digitValue(num[pos]);
                if (d < 0 || d >= base)
                    throw std::invalid_argument(
                        "BigInt: invalid digit in: " + num);
                chunk_value = chunk_value * ubase + static_cast<uint64_t>(d);
                chunk_mult *= ubase;
            }
            mulAddInPlace(chunk_mult, chunk_value);
            take = chunk_len;
        }
        normalize();
        negative = neg && !limbs.empty();
    }
```

Add near `to_int` in the public section:

```cpp
    std::string to_binary_string() const {
        if (isZero()) return "0";
        std::string out = negative ? "-" : "";
        int bit = 63 - std::countl_zero(limbs.back());
        for (size_t i = limbs.size(); i-- > 0;) {
            for (; bit >= 0; --bit)
                out += ((limbs[i] >> bit) & 1) ? '1' : '0';
            bit = 63;
        }
        return out;
    }
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build build --target blockchain_tests -j 8`, then `build\tests\blockchain_tests.exe "[bigint2]"`
Expected: `All tests passed` (11 test cases).

- [ ] **Step 5: Commit**

```bash
git add src/include/bigint_v2.h tests/unit/bigint_v2_test.cpp
git commit -m "feat: bigint v2 string parsing and binary output"
```

---

### Task 5: Division, modulo (floored), and to_string

**Files:**
- Modify: `src/include/bigint_v2.h`
- Modify: `tests/unit/bigint_v2_test.cpp`

**Interfaces:**
- Consumes: everything above; tests build fixtures with `*` and `+`.
- Produces: private `divModMagnitude(a, b, q, r)`; public `operator/`, `operator%` (BigInt and long long), `operator/=`, `operator%=` (BigInt), `operator/=(long long)`, `to_string()`.

- [ ] **Step 1: Write failing tests** (append)

```cpp
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
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build --target blockchain_tests -j 8`
Expected: FAIL to compile — `no match for 'operator/'`.

- [ ] **Step 3: Implement**

Add to the private section — Knuth TAOCP Vol. 2 Algorithm D with a single-limb fast path:

```cpp
    // Magnitude division: q = a / b, r = a % b (truncating, magnitudes only).
    // Precondition: b is normalized and non-empty (checked by callers).
    static void divModMagnitude(const std::vector<uint64_t>& a,
                                const std::vector<uint64_t>& b,
                                std::vector<uint64_t>& quotient,
                                std::vector<uint64_t>& remainder) {
        quotient.clear();
        remainder.clear();
        if (compareMagnitude(a, b) < 0) {
            remainder = a;
            return;
        }

        if (b.size() == 1) {
            const uint64_t d = b[0];
            quotient.assign(a.size(), 0);
            unsigned __int128 rem = 0;
            for (size_t i = a.size(); i-- > 0;) {
                unsigned __int128 cur = (rem << 64) | a[i];
                quotient[i] = static_cast<uint64_t>(cur / d);
                rem = cur % d;
            }
            if (rem != 0) remainder.push_back(static_cast<uint64_t>(rem));
            while (!quotient.empty() && quotient.back() == 0) quotient.pop_back();
            return;
        }

        // Knuth Algorithm D. Normalize so the divisor's top bit is set,
        // divide limb-by-limb with a two-limb trial quotient, correct it
        // (at most twice), multiply-subtract, and denormalize the remainder.
        const size_t n = b.size();
        const size_t m = a.size() - n;
        const int shift = std::countl_zero(b.back());
        const unsigned __int128 BASE = static_cast<unsigned __int128>(1) << 64;

        std::vector<uint64_t> vn(n);
        std::vector<uint64_t> un(a.size() + 1, 0);
        for (size_t i = n; i-- > 0;) {
            vn[i] = b[i] << shift;
            if (shift != 0 && i > 0) vn[i] |= b[i - 1] >> (64 - shift);
        }
        for (size_t i = a.size(); i-- > 0;) {
            un[i] = a[i] << shift;
            if (shift != 0 && i > 0) un[i] |= a[i - 1] >> (64 - shift);
        }
        if (shift != 0) un[a.size()] = a.back() >> (64 - shift);

        quotient.assign(m + 1, 0);
        for (size_t j = m + 1; j-- > 0;) {
            unsigned __int128 numerator =
                (static_cast<unsigned __int128>(un[j + n]) << 64) | un[j + n - 1];
            unsigned __int128 qhat = numerator / vn[n - 1];
            unsigned __int128 rhat = numerator % vn[n - 1];
            while (qhat >= BASE ||
                   qhat * vn[n - 2] > ((rhat << 64) | un[j + n - 2])) {
                --qhat;
                rhat += vn[n - 1];
                if (rhat >= BASE) break;
            }

            // un[j .. j+n] -= qhat * vn[0 .. n-1]
            unsigned __int128 mul_carry = 0;
            __int128 borrow = 0;
            for (size_t i = 0; i < n; ++i) {
                unsigned __int128 product = qhat * vn[i] + mul_carry;
                mul_carry = product >> 64;
                __int128 diff = static_cast<__int128>(un[i + j]) -
                                static_cast<uint64_t>(product) - borrow;
                borrow = diff < 0 ? 1 : 0;
                un[i + j] = static_cast<uint64_t>(diff);   // wraps mod 2^64
            }
            __int128 top = static_cast<__int128>(un[j + n]) -
                           static_cast<__int128>(mul_carry) - borrow;
            un[j + n] = static_cast<uint64_t>(top);

            if (top < 0) {
                // qhat was one too large: add the divisor back.
                --qhat;
                unsigned __int128 carry = 0;
                for (size_t i = 0; i < n; ++i) {
                    unsigned __int128 sum =
                        static_cast<unsigned __int128>(un[i + j]) + vn[i] + carry;
                    un[i + j] = static_cast<uint64_t>(sum);
                    carry = sum >> 64;
                }
                un[j + n] = static_cast<uint64_t>(
                    static_cast<unsigned __int128>(un[j + n]) + carry);
            }
            quotient[j] = static_cast<uint64_t>(qhat);
        }

        remainder.assign(n, 0);
        for (size_t i = 0; i < n; ++i) {
            remainder[i] = un[i] >> shift;
            if (shift != 0) remainder[i] |= un[i + 1] << (64 - shift);
        }
        while (!remainder.empty() && remainder.back() == 0) remainder.pop_back();
        while (!quotient.empty() && quotient.back() == 0) quotient.pop_back();
    }
```

Add to the public section:

```cpp
    BigInt operator/(const BigInt& num) const {
        if (num.isZero())
            throw std::logic_error("Attempted division by zero");
        BigInt q, r;
        divModMagnitude(limbs, num.limbs, q.limbs, r.limbs);
        q.negative = (negative != num.negative) && !q.limbs.empty();
        // Floored: when signs differ and division is inexact, round down.
        if (negative != num.negative && !r.limbs.empty()) q = q - BigInt(1);
        q.normalize();
        return q;
    }

    BigInt operator%(const BigInt& num) const {
        if (num.isZero())
            throw std::logic_error("Attempted division by zero");
        BigInt q, r;
        divModMagnitude(limbs, num.limbs, q.limbs, r.limbs);
        r.negative = negative && !r.limbs.empty();
        // Floored: remainder takes the divisor's sign.
        if (!r.limbs.empty() && r.negative != num.negative) r = r + num;
        r.normalize();
        return r;
    }

    BigInt operator/(const long long& num) const { return *this / BigInt(num); }
    BigInt operator%(const long long& num) const { return *this % BigInt(num); }
    BigInt& operator/=(const BigInt& num) { return *this = *this / num; }
    BigInt& operator%=(const BigInt& num) { return *this = *this % num; }
    BigInt& operator/=(const long long& num) { return *this = *this / BigInt(num); }

    std::string to_string() const {
        if (isZero()) return "0";
        // Peel 19 decimal digits at a time with the single-limb fast path.
        constexpr uint64_t chunk = 10000000000000000000ULL;   // 10^19
        std::vector<uint64_t> mag = limbs;
        std::vector<uint64_t> chunks;
        while (!mag.empty()) {
            unsigned __int128 rem = 0;
            for (size_t i = mag.size(); i-- > 0;) {
                unsigned __int128 cur = (rem << 64) | mag[i];
                mag[i] = static_cast<uint64_t>(cur / chunk);
                rem = cur % chunk;
            }
            while (!mag.empty() && mag.back() == 0) mag.pop_back();
            chunks.push_back(static_cast<uint64_t>(rem));
        }
        std::string out = std::to_string(chunks.back());
        for (size_t i = chunks.size() - 1; i-- > 0;) {
            std::string part = std::to_string(chunks[i]);
            out += std::string(19 - part.size(), '0') + part;
        }
        return negative ? "-" + out : out;
    }
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build build --target blockchain_tests -j 8`, then `build\tests\blockchain_tests.exe "[bigint2]"`
Expected: `All tests passed` (14 test cases, including the 200-iteration invariant loop).

- [ ] **Step 5: Commit**

```bash
git add src/include/bigint_v2.h tests/unit/bigint_v2_test.cpp
git commit -m "feat: bigint v2 Knuth division, floored semantics, to_string"
```

---

### Task 6: Free functions — abs, pow, squared, inverse

**Files:**
- Modify: `src/include/bigint_v2.h`
- Modify: `tests/unit/bigint_v2_test.cpp`

**Interfaces:**
- Consumes: all class operators.
- Produces: `bigint2::abs(BigInt)`, `bigint2::pow(BigInt, int)`, `bigint2::squared(BigInt)`, `bigint2::inverse(BigInt a, BigInt mod)` — the exact free-function set `ECCrypto` calls unqualified after the swap.

- [ ] **Step 1: Write failing tests** (append)

```cpp
TEST_CASE("v2: abs, squared, pow with legacy quirks", "[unit][bigint2]") {
    using bigint2::abs;
    using bigint2::pow;
    using bigint2::squared;
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

TEST_CASE("v2: modular inverse against secp256k1 fixtures", "[unit][bigint2]") {
    using bigint2::inverse;
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
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build build --target blockchain_tests -j 8`
Expected: FAIL to compile — `'abs' is not a member of 'bigint2'`.

- [ ] **Step 3: Implement**

Add after the class body, inside `namespace bigint2`, before the closing brace:

```cpp
inline BigInt abs(const BigInt& num) {
    return num < 0 ? -num : num;
}

// Preserves the legacy contract: negative exponents return `base` when
// |base| == 1, otherwise 0 (integer reciprocal); 0^0 and 0^-n throw.
inline BigInt pow(const BigInt& base, int exp) {
    if (exp < 0) {
        if (base == 0)
            throw std::logic_error("Cannot divide by zero");
        return abs(base) == 1 ? base : BigInt(0);
    }
    if (exp == 0) {
        if (base == 0)
            throw std::logic_error("Zero cannot be raised to zero");
        return BigInt(1);
    }
    BigInt result(1);
    BigInt b = base;
    while (exp > 0) {
        if (exp & 1) result *= b;
        exp >>= 1;
        if (exp != 0) b *= b;
    }
    return result;
}

inline BigInt squared(const BigInt& base) {
    return base * base;
}

// Modular inverse via the extended Euclidean algorithm. The input is
// reduced with floored % first, so negative `a` works; the result is
// normalized into [0, mod) the same way.
inline BigInt inverse(const BigInt& a, const BigInt& mod) {
    BigInt temp_mod = mod;
    BigInt temp_a = a % mod;
    BigInt y_prev(0);
    BigInt y(1);
    while (temp_a > 1) {
        BigInt q = temp_mod / temp_a;
        BigInt y_next = y_prev - q * y;
        y_prev = y;
        y = y_next;
        BigInt r = temp_mod % temp_a;
        temp_mod = temp_a;
        temp_a = r;
    }
    return y % mod;
}
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build build --target blockchain_tests -j 8`, then `build\tests\blockchain_tests.exe "[bigint2]"`
Expected: `All tests passed` (16 test cases).

- [ ] **Step 5: Commit**

```bash
git add src/include/bigint_v2.h tests/unit/bigint_v2_test.cpp
git commit -m "feat: bigint v2 abs, pow, squared, modular inverse"
```

---

### Task 7: The swap — replace bigint.h, delete scaffolding, measure

**Files:**
- Modify: `src/include/bigint.h` (full replacement)
- Delete: `src/include/bigint_v2.h`
- Rename: `tests/unit/bigint_v2_test.cpp` → `tests/unit/bigint_ops_test.cpp` (tags and includes updated)
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: the completed `bigint2::BigInt`.
- Produces: global `::BigInt` and free functions `abs`, `pow`, `squared`, `inverse` in `src/include/bigint.h` — exactly what `ECCrypto.h`/`ECCrypto.cpp` and `tests/unit/bigint_test.cpp` already reference.

- [ ] **Step 1: Record the baseline timing (old implementation)**

```powershell
Measure-Command { build\tests\blockchain_tests.exe 2>$null } | Select-Object TotalSeconds
```

Note the value (expect roughly 120–130 s; ECC dominates).

- [ ] **Step 2: Replace `src/include/bigint.h`**

Overwrite `src/include/bigint.h` with the entire contents of `src/include/bigint_v2.h`, with exactly two changes: delete the `namespace bigint2 {` line and the closing `} // namespace bigint2` line (class and free functions become global, as the old header had them). Keep `#pragma once` and all includes. Then delete `src/include/bigint_v2.h`.

- [ ] **Step 3: Move the test file to its permanent name**

Rename `tests/unit/bigint_v2_test.cpp` to `tests/unit/bigint_ops_test.cpp` and update its header block: change `#include "bigint_v2.h"` to `#include "bigint.h"`, delete the `using bigint2::BigInt;` line, delete the `using bigint2::abs;`-style lines inside test cases, and replace every `[bigint2]` tag with `[bigint]`. In `tests/CMakeLists.txt`, change `unit/bigint_v2_test.cpp` to `unit/bigint_ops_test.cpp`.

Note: `abs`/`pow` calls in the test file must stay unqualified — they now resolve to the global functions, proving the drop-in surface.

- [ ] **Step 4: Full rebuild and full suite**

```powershell
cmake --build build -j 8
build\tests\blockchain_tests.exe
```

Expected: `All tests passed` — every existing test (`bigint_test.cpp`'s 5 cases, ECC, transaction, wallet, blockchain, lifecycle, P2P) plus the 16 renamed cases, with **zero modifications to any file outside this task's list**. Verify that with `git status --short` — only the four planned files may appear.

- [ ] **Step 5: Measure the new timing**

```powershell
Measure-Command { build\tests\blockchain_tests.exe 2>$null } | Select-Object TotalSeconds
```

Expected: a large drop (the ECC-heavy portion should collapse from ~2 minutes toward seconds).

- [ ] **Step 6: Commit with the numbers**

```bash
git add -A src/include tests
git commit -m "feat: replace vendored BigInt with original 64-bit-limb implementation

Sign-magnitude limbs with __int128 arithmetic, Knuth Algorithm D
division, floored division semantics preserved exactly. Header-only
drop-in: no caller changes. Full suite: <before> s -> <after> s."
```

(Replace `<before>`/`<after>` with the measured values from Steps 1 and 5.)

---

## Self-Review

- **Spec coverage:** representation (Task 1), semantics contract (Tasks 1/5 tests), all algorithms table rows (Tasks 2–6), API surface (Tasks 1–6, proven by Task 7 compiling `ECCrypto` unchanged), testing list (floored sign combos ✓, 2^64/2^128 carries ✓, Knuth-D stress ✓, base-2/16 parsing ✓, binary round-trip ✓, `to_int` range ✓, secp256k1 inverse fixtures ✓), timing record (Task 7).
- **Placeholders:** none; every step carries complete code or exact commands.
- **Type consistency:** helper names (`compareMagnitude`, `addMagnitude`, `subMagnitude`, `mulMagnitude`, `divModMagnitude`, `mulAddInPlace`, `digitValue`, `normalize`, `isZero`, `compare`) are used consistently across tasks; free functions live in `bigint2` until Task 7 strips the namespace.
