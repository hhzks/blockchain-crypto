#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// Monetary values are an integer count of the smallest unit, never a double.
// Binary floating point cannot represent most decimal fractions exactly and
// its addition is not associative, which mattered here in concrete ways: a
// consensus rule compared a reward with ==, serialisation round-tripped
// amounts through a fixed-precision decimal rendering, two balance routines
// summed in different orders, and NaN was representable at all.
namespace money {

using Amount = std::int64_t;

// 1 coin = 1e8 units, the same scale Bitcoin uses.
constexpr Amount COIN = 100'000'000;

// A supply cap, so sums of amounts cannot overflow int64 in any realistic
// chain and every amount has a stated upper bound to validate against.
constexpr Amount MAX_MONEY = 21'000'000 * COIN;

constexpr Amount coins(std::int64_t whole) { return whole * COIN; }

// What a transaction amount may be: strictly positive and within the cap.
constexpr bool isValidAmount(Amount amount) {
    return amount > 0 && amount <= MAX_MONEY;
}

// Renders as a decimal coin figure with a fixed eight places. Balances may be
// negative, so this handles a sign; transaction amounts may not.
std::string format(Amount amount);

// Parses a decimal coin figure ("1", "1.5", "0.00000001") into units.
// Rejects anything that is not one positive in-range value, including
// precision finer than a single unit, which would otherwise be silently lost.
std::optional<Amount> parse(std::string_view text);

} // namespace money
