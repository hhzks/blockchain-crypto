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
