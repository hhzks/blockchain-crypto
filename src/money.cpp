#include "include/money.h"

#include <charconv>
#include <cstdlib>
#include <format>

namespace money {

namespace {

constexpr int DECIMALS = 8;

std::string_view trim(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

bool allDigits(std::string_view text) {
    if (text.empty()) return false;
    for (char c : text) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

} // namespace

std::string format(Amount amount) {
    // Formatting the magnitude separately keeps -1 rendering as "-0.00000001"
    // rather than "0.-0000001".
    const bool negative = amount < 0;
    const std::uint64_t magnitude =
        negative ? static_cast<std::uint64_t>(-(amount + 1)) + 1
                 : static_cast<std::uint64_t>(amount);

    const std::uint64_t whole = magnitude / static_cast<std::uint64_t>(COIN);
    const std::uint64_t fraction = magnitude % static_cast<std::uint64_t>(COIN);

    return std::format("{}{}.{:08}", negative ? "-" : "", whole, fraction);
}

std::optional<Amount> parse(std::string_view text) {
    text = trim(text);
    if (text.empty()) {
        return std::nullopt;
    }

    const auto point = text.find('.');
    const std::string_view whole_text =
        point == std::string_view::npos ? text : text.substr(0, point);
    const std::string_view fraction_text =
        point == std::string_view::npos ? std::string_view{} : text.substr(point + 1);

    // Digits only, so "1e8", "nan", "inf", "-1" and "1.2.3" are all refused
    // rather than partially parsed.
    if (!allDigits(whole_text)) {
        return std::nullopt;
    }
    if (point != std::string_view::npos && !allDigits(fraction_text)) {
        return std::nullopt;
    }
    if (fraction_text.size() > static_cast<size_t>(DECIMALS)) {
        return std::nullopt; // finer than one unit; would be silently lost
    }

    std::uint64_t whole = 0;
    const auto [ptr, ec] =
        std::from_chars(whole_text.data(), whole_text.data() + whole_text.size(), whole);
    if (ec != std::errc{} || ptr != whole_text.data() + whole_text.size()) {
        return std::nullopt;
    }

    if (whole > static_cast<std::uint64_t>(MAX_MONEY / COIN)) {
        return std::nullopt;
    }

    std::uint64_t fraction = 0;
    if (!fraction_text.empty()) {
        std::uint64_t scale = 1;
        for (size_t i = fraction_text.size(); i < static_cast<size_t>(DECIMALS); i++) {
            scale *= 10;
        }

        std::uint64_t digits = 0;
        const auto [fptr, fec] = std::from_chars(
            fraction_text.data(), fraction_text.data() + fraction_text.size(), digits);
        if (fec != std::errc{} || fptr != fraction_text.data() + fraction_text.size()) {
            return std::nullopt;
        }
        fraction = digits * scale;
    }

    const std::uint64_t total = whole * static_cast<std::uint64_t>(COIN) + fraction;
    if (total > static_cast<std::uint64_t>(MAX_MONEY)) {
        return std::nullopt;
    }

    return static_cast<Amount>(total);
}

} // namespace money
