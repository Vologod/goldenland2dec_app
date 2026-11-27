#include <doctest/doctest.h>

#include "gl/game/person_runtime.hpp"

TEST_CASE("PersonRuntime appends effects and writes raw parameters") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);

    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x40, 0, -1};
    runtime.add_effect("Hero", effect);
    REQUIRE(runtime.effects("Hero").size() == 1);
    CHECK(runtime.effects("Hero")[0].words[0] == 9);
    CHECK(runtime.effects("Hero")[0].words[1] == -3);

    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::reputation, 40.0);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::reputation) ==
          doctest::Approx(40.0));

    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::strength, 12.0);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::strength) ==
          doctest::Approx(12.0));

    runtime.add_effect("missing", effect);
    runtime.set_raw_parameter("missing", gl::script::PersonParameter::strength, 5.0);
}

#include "gl/game/inventory_runtime.hpp"

TEST_CASE("InventoryRuntime consumes one item from the selected hero slot") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back(gl::assets::InventoryItem(953, 3, 0, 19, 0));
    inventory.hot.push_back(gl::assets::InventoryItem(948, 1, 0, 19, 0));
    auto runtime = gl::game::InventoryRuntime::create(inventory, {}, {});

    CHECK(runtime.has_hero_item(gl::game::HeroItemContainer::main, 0));
    CHECK(runtime.has_hero_item(gl::game::HeroItemContainer::hot, 0));
    CHECK_FALSE(runtime.has_hero_item(gl::game::HeroItemContainer::main, 5));

    CHECK(runtime.consume_hero_item(gl::game::HeroItemContainer::main, 0));
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].count == 2);

    CHECK(runtime.consume_hero_item(gl::game::HeroItemContainer::hot, 0));
    CHECK(runtime.hero_inventory().hot.empty());
    CHECK_FALSE(runtime.consume_hero_item(gl::game::HeroItemContainer::hot, 0));
}
