#include <catch2/catch_test_macros.hpp>
#include <limits>
#include "money.h"

using money::Amount;
using money::COIN;

TEST_CASE("money::format renders a fixed eight decimal places",
          "[unit][money]") {
    REQUIRE(money::format(0) == "0.00000000");
    REQUIRE(money::format(1) == "0.00000001");
    REQUIRE(money::format(COIN) == "1.00000000");
    REQUIRE(money::format(COIN / 2) == "0.50000000");
    REQUIRE(money::format(50 * COIN) == "50.00000000");
    REQUIRE(money::format(money::MAX_MONEY) == "21000000.00000000");
}

TEST_CASE("money::format handles negative amounts", "[unit][money]") {
    // Balances can legitimately go negative in reporting, even though a
    // transaction amount cannot.
    REQUIRE(money::format(-1) == "-0.00000001");
    REQUIRE(money::format(-COIN) == "-1.00000000");
}

TEST_CASE("money::parse accepts decimal coin amounts", "[unit][money]") {
    REQUIRE(money::parse("1") == COIN);
    REQUIRE(money::parse("1.5") == COIN + COIN / 2);
    REQUIRE(money::parse("0.00000001") == 1);
    REQUIRE(money::parse("50.00000000") == 50 * COIN);
    REQUIRE(money::parse("  2.25  ") == 2 * COIN + COIN / 4);
    REQUIRE(money::parse("21000000") == money::MAX_MONEY);
}

TEST_CASE("money::parse rejects what is not a positive coin amount",
          "[unit][money]") {
    REQUIRE_FALSE(money::parse("").has_value());
    REQUIRE_FALSE(money::parse("abc").has_value());
    REQUIRE_FALSE(money::parse("1.2.3").has_value());
    REQUIRE_FALSE(money::parse("1e8").has_value());
    REQUIRE_FALSE(money::parse("nan").has_value());
    REQUIRE_FALSE(money::parse("inf").has_value());
    REQUIRE_FALSE(money::parse("-1").has_value());

    // More precision than the smallest unit can hold would be silently lost.
    REQUIRE_FALSE(money::parse("0.000000001").has_value());

    // Beyond the supply cap.
    REQUIRE_FALSE(money::parse("21000001").has_value());
}

TEST_CASE("money::parse and money::format round-trip", "[unit][money]") {
    for (Amount amount : {Amount{1}, COIN, 50 * COIN, COIN / 3, money::MAX_MONEY}) {
        INFO("amount = " << amount);
        REQUIRE(money::parse(money::format(amount)) == amount);
    }
}

TEST_CASE("money::isValidAmount bounds a transaction amount",
          "[unit][money]") {
    REQUIRE(money::isValidAmount(1));
    REQUIRE(money::isValidAmount(money::MAX_MONEY));

    REQUIRE_FALSE(money::isValidAmount(0));
    REQUIRE_FALSE(money::isValidAmount(-1));
    REQUIRE_FALSE(money::isValidAmount(money::MAX_MONEY + 1));
    REQUIRE_FALSE(money::isValidAmount(std::numeric_limits<Amount>::max()));
}
