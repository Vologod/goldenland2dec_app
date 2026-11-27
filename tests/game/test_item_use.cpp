#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "gl/assets/hero_stats.hpp"
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

#include "gl/game/item_use.hpp"

namespace {
gl::assets::ItemEffect make_effect(std::uint32_t type, std::int32_t value, std::uint32_t flags,
                                   std::uint8_t dur_hi = 0, std::uint8_t dur_lo = 0,
                                   std::uint8_t delay_hi = 0, std::uint8_t delay_lo = 0) {
    gl::assets::ItemEffect effect;
    effect.type = type;
    effect.amount = static_cast<std::uint32_t>(value);
    effect.flags = flags;
    effect.duration.hi = dur_hi;
    effect.duration.lo = dur_lo;
    effect.delay.hi = delay_hi;
    effect.delay.lo = delay_lo;
    return effect;
}

gl::assets::Item make_item(gl::assets::ItemType type, std::uint32_t id,
                           std::vector<gl::assets::ItemEffect> effects = {},
                           std::uint32_t heal_hp = 0, std::uint32_t heal_ep = 0) {
    gl::assets::Item item;
    item.type = type;
    item.description_id = id;
    item.effects = std::move(effects);
    item.healing_health = heal_hp;
    item.healing_mana = heal_ep;
    return item;
}

gl::assets::InventoryItem inventory_item(std::uint32_t id, std::uint32_t type,
                                         std::uint32_t count) {
    return gl::assets::InventoryItem(id, count, 0, type, 0);
}

gl::game::HeroUseResult use_single(const gl::assets::Item &item, gl::assets::HeroStats &stats,
                                   gl::game::PersonRuntime &runtime,
                                   gl::game::InventoryRuntime &inventory) {
    return gl::game::use_hero_item(item, gl::game::HeroItemContainer::main, 0, stats, runtime,
                                   inventory);
}
} // namespace

TEST_CASE("is_consumable accepts potions and food only") {
    CHECK(gl::game::is_consumable(gl::assets::ItemType::potion));
    CHECK(gl::game::is_consumable(gl::assets::ItemType::food));
    CHECK_FALSE(gl::game::is_consumable(gl::assets::ItemType::sword));
    CHECK_FALSE(gl::game::is_consumable(gl::assets::ItemType::scroll));
}

TEST_CASE("use_hero_item appends timed effects with packed cond") {
    gl::assets::Item item = make_item(gl::assets::ItemType::food, 408,
                                      {make_effect(30, 100, 0x50, 3, 0)});
    gl::assets::HeroStats stats;
    gl::assets::HeroInventory hero;
    hero.items.push_back(inventory_item(408, 20, 1));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    const auto result = use_single(item, stats, runtime, inventory);
    CHECK(result.used);
    CHECK(result.consumed);
    CHECK(result.effects_added == 1);
    REQUIRE(runtime.effects("Hero").size() == 1);
    CHECK(runtime.effects("Hero")[0].words[0] == 30);
    CHECK(runtime.effects("Hero")[0].words[1] == 100);
    CHECK(static_cast<std::uint32_t>(runtime.effects("Hero")[0].words[2]) == 0x50);
    CHECK(static_cast<std::uint32_t>(runtime.effects("Hero")[0].words[3]) == 0x00000003);
    CHECK(runtime.effects("Hero")[0].words[4] == -1);
    CHECK(inventory.hero_inventory().items.empty());
}

TEST_CASE("use_hero_item applies direct stats with clamps") {
    gl::assets::Item item = make_item(gl::assets::ItemType::potion, 948,
                                      {make_effect(6, 1, 0x41), make_effect(10, -1, 0x41)});
    gl::assets::HeroStats stats;
    stats.strength = 10;
    stats.intelligence = 10;
    gl::assets::HeroInventory hero;
    hero.items.push_back(inventory_item(948, 19, 1));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    const auto result = use_single(item, stats, runtime, inventory);
    CHECK(result.used);
    CHECK(result.effects_added == 0);
    CHECK(stats.strength == 11);
    CHECK(stats.intelligence == 9);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::strength) ==
          doctest::Approx(11.0));
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::intelligence) ==
          doctest::Approx(9.0));

    stats.strength = 30;
    gl::assets::HeroInventory hero2;
    hero2.items.push_back(inventory_item(948, 19, 1));
    auto inventory2 = gl::game::InventoryRuntime::create(hero2, {}, {});
    auto runtime2 = gl::game::PersonRuntime::create({}, stats);
    use_single(item, stats, runtime2, inventory2);
    CHECK(stats.strength == 30);

    stats.strength = 1;
    gl::assets::HeroInventory hero3;
    hero3.items.push_back(inventory_item(948, 19, 1));
    auto inventory3 = gl::game::InventoryRuntime::create(hero3, {}, {});
    auto runtime3 = gl::game::PersonRuntime::create({}, stats);
    use_single(make_item(gl::assets::ItemType::potion, 948, {make_effect(6, -1, 0x41)}), stats,
               runtime3, inventory3);
    CHECK(stats.strength == 1);
}

TEST_CASE("use_hero_item clamps reputation") {
    gl::assets::Item item = make_item(gl::assets::ItemType::food, 410,
                                      {make_effect(28, -1, 0x41)});
    gl::assets::HeroStats stats;
    stats.reputation = -50;
    gl::assets::HeroInventory hero;
    hero.items.push_back(inventory_item(410, 20, 1));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);
    use_single(item, stats, runtime, inventory);
    CHECK(stats.reputation == -50);
}

TEST_CASE("use_hero_item heals HP and EP with clamp") {
    gl::assets::Item item = make_item(gl::assets::ItemType::potion, 957, {}, 20, 20);
    gl::assets::HeroStats stats;
    stats.life = 10;
    stats.energy = 10;
    stats.experience = 1000;
    gl::assets::HeroInventory hero;
    hero.items.push_back(inventory_item(957, 19, 1));
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});
    auto runtime = gl::game::PersonRuntime::create({}, stats);
    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::life, 10.0);
    runtime.set_raw_parameter("Hero", gl::script::PersonParameter::energy, 10.0);
    const double max_life =
        runtime.get_parameter("Hero", gl::script::PersonParameter::maximum_life);
    const double max_energy = runtime.maximum_energy("Hero");

    const auto result = use_single(item, stats, runtime, inventory);
    CHECK(result.used);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::life) ==
          doctest::Approx(std::min(max_life, 30.0)));
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::energy) ==
          doctest::Approx(std::min(max_energy, 30.0)));
    CHECK(stats.life == static_cast<std::int32_t>(std::min(max_life, 30.0)));
    CHECK(stats.energy == static_cast<std::int32_t>(std::min(max_energy, 30.0)));
}

TEST_CASE("use_hero_item rejects non-consumables and bad slots") {
    gl::assets::HeroStats stats;
    auto runtime = gl::game::PersonRuntime::create({}, stats);
    gl::assets::HeroInventory hero;
    auto inventory = gl::game::InventoryRuntime::create(hero, {}, {});

    const auto sword = use_single(make_item(gl::assets::ItemType::sword, 1), stats, runtime,
                                  inventory);
    CHECK_FALSE(sword.used);
    CHECK_FALSE(sword.consumed);

    const auto potion = use_single(make_item(gl::assets::ItemType::potion, 953), stats, runtime,
                                   inventory);
    CHECK_FALSE(potion.used);
}

#include "gl/game/hero_save_write.hpp"

TEST_CASE("make_hero_save_body writes stats and preserves the rest of the block") {
    gl::assets::HeroSaveBody body;
    body.stat_block.fill(0xAA);
    gl::game::HeroLiveState live;
    gl::assets::HeroStats stats;
    stats.strength = 11;
    stats.constitution = 12;
    stats.dexterity = 13;
    stats.perception = 14;
    stats.intelligence = 15;
    stats.wisdom = 16;
    stats.luck = 17;
    stats.reputation = 18;
    stats.experience = 12345;
    stats.skills[0] = 5;
    stats.skills[26] = 9;
    live.stats = stats;

    const auto result = gl::game::make_hero_save_body(body, live);
    const auto read = [&](std::size_t offset) {
        return static_cast<std::uint32_t>(result.stat_block[offset]) |
               (static_cast<std::uint32_t>(result.stat_block[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(result.stat_block[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(result.stat_block[offset + 3]) << 24);
    };
    CHECK(read(0) == 11);
    CHECK(read(4) == 12);
    CHECK(read(8) == 13);
    CHECK(read(0x0C) == 14);
    CHECK(read(0x10) == 15);
    CHECK(read(0x14) == 16);
    CHECK(read(0x18) == 17);
    CHECK(read(0x1C) == 18);
    CHECK(read(0x20) == 12345);
    CHECK(read(0x2C) == 5);
    CHECK(read(0x2C + 4 * 26) == 9);
    CHECK(result.stat_block[0x100] == 0xAA);
}