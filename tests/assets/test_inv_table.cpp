#include "gl/assets/bytes.hpp"
#include "gl/assets/inv_table.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
Bytes to_bytes(std::string_view text) { return Bytes(text.begin(), text.end()); }

gl::Result<InvTable> parse(std::string_view text) { return parse_inv(to_bytes(text)); }
} // namespace

TEST_CASE("inv_table: parses headers and five-number items") {
    auto table = parse("level_offset 4\n"
                       "regenerate_chance 75\n"
                       "chance_decrease 2\n"
                       "quantity_decrease 1\n"
                       "item \"MON_1_0_1\" -100 10 100 50 100\n"
                       "item \"SWR_1_02_2\" 1 100 0.5 5 7\n");
    REQUIRE(table.has_value());
    CHECK(table->level_offset == 4);
    CHECK(table->regenerate_chance == doctest::Approx(0.75));
    CHECK(table->chance_decrease == doctest::Approx(2.0));
    CHECK(table->quantity_decrease == doctest::Approx(1.0));
    REQUIRE(table->items.size() == 2);

    CHECK(table->items[0].tech_name == "MON_1_0_1");
    CHECK(table->items[0].min_level == -100);
    CHECK(table->items[0].max_level == 10);
    CHECK(table->items[0].chance_percent == doctest::Approx(100.0));
    CHECK(table->items[0].qty_min == 50);
    CHECK(table->items[0].qty_max == 100);

    CHECK(table->items[1].tech_name == "SWR_1_02_2");
    CHECK(table->items[1].min_level == 1);
    CHECK(table->items[1].max_level == 100);
    CHECK(table->items[1].chance_percent == doctest::Approx(0.5));
    CHECK(table->items[1].qty_min == 5);
    CHECK(table->items[1].qty_max == 7);
}

TEST_CASE("inv_table: skips four-number items and keeps later records") {
    auto table = parse("regenerate_chance 1\n"
                       "chance_decrease 1\n"
                       "quantity_decrease 1\n"
                       "item \"FOUR\" 1 100 100 2\n"
                       "item \"FIVE\" 1 100 100 2 3\n");
    REQUIRE(table.has_value());
    REQUIRE(table->items.size() == 1);
    CHECK(table->items[0].tech_name == "FIVE");
    CHECK(table->items[0].qty_min == 2);
    CHECK(table->items[0].qty_max == 3);
}

TEST_CASE("inv_table: supports whitespace-separated comments") {
    auto table = parse("// standalone comment\n"
                       "regenerate_chance 50 // trailing value comment\n"
                       "item \"X\" 1 2 50 3 4 // trailing item comment\n");
    REQUIRE(table.has_value());
    CHECK(table->regenerate_chance == doctest::Approx(0.5));
    REQUIRE(table->items.size() == 1);
    CHECK(table->items[0].tech_name == "X");
}

TEST_CASE("inv_table: attached comment merges tokens like the original lexer") {
    auto table = parse("regenerate_chance 5//ooops\n"
                       "item \"X\" 1 2 3 4 5\n");
    REQUIRE(table.has_value());
    CHECK(table->regenerate_chance == doctest::Approx(1.0));
    CHECK(table->items.empty());
}

TEST_CASE("inv_table: quoted names keep inner spaces") {
    auto table = parse("item \"TECH NAME\" 1 2 3 4 5\n");
    REQUIRE(table.has_value());
    REQUIRE(table->items.size() == 1);
    CHECK(table->items[0].tech_name == "TECH NAME");
}

TEST_CASE("inv_table: ignores special declarations") {
    auto table = parse("special SOMETHING\n"
                       "item \"A\" 1 2 3 4 5\n");
    REQUIRE(table.has_value());
    REQUIRE(table->items.size() == 1);
    CHECK(table->items[0].tech_name == "A");
}

TEST_CASE("inv_table: malformed headers leave the clear defaults") {
    auto table = parse("regenerate_chance nope\n"
                       "chance_decrease 2\n"
                       "quantity_decrease -\n"
                       "level_offset xyz\n"
                       "item \"A\" 1 2 3 4 5\n");
    REQUIRE(table.has_value());
    CHECK(table->regenerate_chance == doctest::Approx(1.0));
    CHECK(table->chance_decrease == doctest::Approx(2.0));
    CHECK(table->quantity_decrease == doctest::Approx(1.0));
    CHECK(table->level_offset == 0);
    REQUIRE(table->items.size() == 1);
}

TEST_CASE("inv_table: empty input yields defaults and no items") {
    auto table = parse("");
    REQUIRE(table.has_value());
    CHECK(table->regenerate_chance == doctest::Approx(1.0));
    CHECK(table->chance_decrease == doctest::Approx(1.0));
    CHECK(table->quantity_decrease == doctest::Approx(1.0));
    CHECK(table->level_offset == 0);
    CHECK(table->items.empty());
}
