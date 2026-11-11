#include "gl/game/trade_price.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>

using namespace gl::game;

namespace {
TradePriceInput boat_trader_input(std::uint32_t skill, std::uint32_t count) {
    TradePriceInput input{};
    input.item_id = 953;
    input.item_price = 500;
    input.durability = 100;
    input.flags = 0x8000000Cu;
    input.mod_count = 0;
    input.skill_trade = skill;
    input.count = count;
    input.assume_x87_single = true;
    return input;
}
} // namespace

TEST_CASE("trade_price boat-trader slot0 stack matches the oracle") {
    const auto price = compute_trade_price(boat_trader_input(1, 14));
    REQUIRE(price.has_value());
    CHECK(price->count == 14);
    CHECK(price->buy == 16800);
    CHECK(price->sell == 2912);
    CHECK(price->buy / 14 == 1200);
    CHECK(price->sell / 14 == 208);
}

TEST_CASE("trade_price boat-trader slot2 stack matches the oracle") {
    const auto price = compute_trade_price(boat_trader_input(5, 17));
    REQUIRE(price.has_value());
    CHECK(price->count == 17);
    CHECK(price->buy == 17000);
    CHECK(price->sell == 4250);
    CHECK(price->buy / 17 == 1000);
    CHECK(price->sell / 17 == 250);
}

TEST_CASE("trade_price slot0 sell stack (count 51) matches the oracle") {
    const auto price = compute_trade_price(boat_trader_input(1, 51));
    REQUIRE(price.has_value());
    CHECK(price->buy == 61200);
    CHECK(price->sell == 10608);
    CHECK(price->sell / 51 == 208);
}

TEST_CASE("trade_price slot2 sell stack (count 25) matches the oracle") {
    const auto price = compute_trade_price(boat_trader_input(5, 25));
    REQUIRE(price.has_value());
    CHECK(price->buy == 25000);
    CHECK(price->sell == 6250);
    CHECK(price->sell / 25 == 250);
}

TEST_CASE("trade_price applies durability factor") {
    TradePriceInput input = boat_trader_input(1, 1);
    input.durability = 50;
    const auto price = compute_trade_price(input);
    REQUIRE(price.has_value());
    CHECK(price->buy == 1048);
    CHECK(price->sell == 182);
}

TEST_CASE("trade_price truncates the base and the unit values") {
    TradePriceInput input = boat_trader_input(1, 1);
    input.item_price = 1000;
    input.durability = 1;
    const auto price = compute_trade_price(input);
    REQUIRE(price.has_value());
    CHECK(price->buy == 1804);
    CHECK(price->sell == 313);
}

TEST_CASE("trade_price clamps unit values to at least one") {
    TradePriceInput input = boat_trader_input(10, 1);
    input.item_price = 1;
    const auto price = compute_trade_price(input);
    REQUIRE(price.has_value());
    CHECK(price->buy == 1);
    CHECK(price->sell == 1);
}

TEST_CASE("trade_price clamps the skill factor to one") {
    const auto price = compute_trade_price(boat_trader_input(20, 1));
    REQUIRE(price.has_value());
    CHECK(price->buy == 500);
    CHECK(price->sell == 500);
}

TEST_CASE("trade_price rejects an undecoded modifier block for buy and sell") {
    TradePriceInput input = boat_trader_input(1, 1);
    input.mod_count = 1;
    input.flags = 0;
    CHECK_FALSE(compute_trade_price(input).has_value());

    input.flags = 0x80;
    CHECK(compute_trade_price(input).has_value());

    input.mod_count = 0;
    input.flags = 0;
    CHECK(compute_trade_price(input).has_value());
}

TEST_CASE("trade_price carries the quoted item id") {
    const auto price = compute_trade_price(boat_trader_input(1, 1));
    REQUIRE(price.has_value());
    CHECK(price->item_id == 953);
}

TEST_CASE("trade_price rejects a non-opted-in x87 single assumption") {
    TradePriceInput input = boat_trader_input(1, 1);
    input.assume_x87_single = false;
    CHECK_FALSE(compute_trade_price(input).has_value());
}

TEST_CASE("trade_price rejects invalid caller inputs") {
    TradePriceInput input = boat_trader_input(1, 1);

    input.item_price = 0;
    CHECK_FALSE(compute_trade_price(input).has_value());

    input = boat_trader_input(1, 1);
    input.durability = 101;
    CHECK_FALSE(compute_trade_price(input).has_value());

    input = boat_trader_input(1, 1);
    input.count = 0;
    CHECK_FALSE(compute_trade_price(input).has_value());
}

TEST_CASE("trade_price rejects a whole-stack total that would overflow int64") {
    TradePriceInput input = boat_trader_input(0, 0xFFFFFFFFu);
    input.item_price = 0xFFFFFFFFu;
    CHECK_FALSE(compute_trade_price(input).has_value());
}

TEST_CASE("trade_price rejects an empty stack") {
    CHECK_FALSE(compute_trade_price(boat_trader_input(1, 0)).has_value());
}

TEST_CASE("trade_price uses x87 single precision for the skill factor") {
    TradePriceInput input{};
    input.item_id = 1;
    input.item_price = 5;
    input.durability = 100;
    input.flags = 0;
    input.mod_count = 0;
    input.skill_trade = 9;
    input.count = 1;
    input.assume_x87_single = true;

    const auto price = compute_trade_price(input);
    REQUIRE(price.has_value());
    CHECK(price->buy == 8);
    CHECK(price->sell == 3);
}

TEST_CASE("trade_price uses double constants with binary32 results") {
    TradePriceInput input{};
    input.item_id = 1;
    input.item_price = 75;
    input.durability = 36;
    input.flags = 0;
    input.mod_count = 0;
    input.skill_trade = 1;
    input.count = 1;
    input.assume_x87_single = true;

    const auto price = compute_trade_price(input);
    REQUIRE(price.has_value());
    CHECK(price->buy == 151);
    CHECK(price->sell == 26);
}
