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

    BigInt operator*(const BigInt& num) const {
        BigInt result;
        result.limbs = mulMagnitude(limbs, num.limbs);
        result.negative = (negative != num.negative) && !result.limbs.empty();
        result.normalize();
        return result;
    }

    BigInt operator*(const long long& num) const { return *this * BigInt(num); }
    BigInt& operator*=(const BigInt& num) { return *this = *this * num; }
};

} // namespace bigint2
