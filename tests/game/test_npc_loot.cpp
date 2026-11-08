#include "gl/game/npc_loot.hpp"

#include "gl/assets/inv_table.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/game/msvc_random.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using gl::game::ItemTemplate;
using gl::game::LootItem;
using gl::game::MsvcRandom;
using gl::game::NpcLoot;
using gl::game::NpcLootGenerator;

namespace {
gl::assets::InvItem make_item(std::string name, std::int32_t min_level, std::int32_t max_level,
                              double chance_percent, std::int32_t qty_min, std::int32_t qty_max) {
    gl::assets::InvItem item;
    item.tech_name = std::move(name);
    item.min_level = min_level;
    item.max_level = max_level;
    item.chance_percent = chance_percent;
    item.qty_min = qty_min;
    item.qty_max = qty_max;
    return item;
}

gl::assets::ItemNameIndex make_names() {
    const std::vector<gl::assets::SdbEntry> entries = {
        {100, "A"},
        {102, "B"},
        {103, "C"},
        {104, "D"},
        {105, "E"},
        {106, "F"},
        {999, "UNKNOWN_IN_INDEX"},
    };
    auto index = gl::assets::ItemNameIndex::from_entries(entries);
    REQUIRE(index.has_value());
    return index.value();
}

std::optional<ItemTemplate> lookup(std::uint32_t item_id) {
    switch (item_id) {
    case 100:
        return ItemTemplate{1, 0x4u, 7, 4};
    case 102:
        return ItemTemplate{2, 0x80u, 3, 3};
    case 103:
        return ItemTemplate{3, 0x40u, 5, 2};
    case 104:
        return ItemTemplate{4, 0, 1, 100};
    case 105:
        return ItemTemplate{5, 0, 2, 1};
    case 106:
        return ItemTemplate{6, 0x84u, 9, 2};
    default:
        return std::nullopt;
    }
}

void check_items(const std::vector<LootItem> &got, const std::vector<LootItem> &want) {
    REQUIRE(got.size() == want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        INFO("item " << i);
        CHECK(got[i].item_id == want[i].item_id);
        CHECK(got[i].count == want[i].count);
        CHECK(got[i].type == want[i].type);
        CHECK(got[i].flags == want[i].flags);
        CHECK(got[i].style == want[i].style);
    }
}
} // namespace

TEST_CASE("npc_loot: empty table consumes only R1") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 0.75;

    MsvcRandom rng(1);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    CHECK(loot.hot.empty());
    CHECK(loot.main.empty());
    CHECK(rng.next() == 18467u);
}

TEST_CASE("npc_loot: records outside the level window consume no RNG") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 0.75;
    table.level_offset = 4;
    table.items = {make_item("OUT", 100, 200, 100, 1, 1)};

    MsvcRandom rng(1);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    CHECK(loot.hot.empty());
    CHECK(loot.main.empty());
    CHECK(rng.next() == 18467u);
}

TEST_CASE("npc_loot: two passes fill hot and main in RNG order") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 0.75;
    table.chance_decrease = 2.0;
    table.quantity_decrease = 1.0;
    table.level_offset = 4;
    table.items = {
        make_item("A", 0, 20, 100, 1, 10),
        make_item("OUT", 100, 200, 100, 9, 9),
        make_item("B", 7, 7, 100, 2, 6),
        make_item("C", 0, 0, 50, 1, 5),
    };

    MsvcRandom rng(1);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    check_items(loot.hot, {{100, 2, 1, 0x4u, 7}, {102, 3, 2, 0x80u, 3}, {102, 1, 2, 0x80u, 3}});
    check_items(loot.main, {{100, 4, 1, 0x4u, 7}});
    CHECK(rng.next() == 26962u);
}

TEST_CASE("npc_loot: pass 2 does not draw chance coin when chance_percent is zero") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 0.5;
    table.chance_decrease = 1.0;
    table.quantity_decrease = 2.0;
    table.items = {make_item("E", 0, 100, 0, 5, 7)};

    MsvcRandom rng(7);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    CHECK(loot.hot.empty());
    check_items(loot.main,
                {{105, 1, 5, 0, 2}, {105, 1, 5, 0, 2}, {105, 1, 5, 0, 2}, {105, 1, 5, 0, 2}});
    CHECK(rng.next() == 15215u);
}

TEST_CASE("npc_loot: pass 2 scales quantity by quantity_decrease") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 1.0;
    table.chance_decrease = 1.0;
    table.quantity_decrease = 3.0;
    table.items = {make_item("D", 0, 100, 100, 5, 5)};

    MsvcRandom rng(3);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    check_items(loot.hot, {{104, 5, 4, 0, 1}});
    check_items(loot.main, {{104, 2, 4, 0, 1}});
    CHECK(rng.next() == 9091u);
}

TEST_CASE("npc_loot: pass 2 scales chance by chance_decrease") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 1.0;
    table.chance_decrease = 1000.0;
    table.quantity_decrease = 1.0;
    table.items = {make_item("D", 0, 100, 100, 1, 1)};

    MsvcRandom rng(5);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    check_items(loot.hot, {{104, 1, 4, 0, 1}});
    CHECK(loot.main.empty());
    CHECK(rng.next() == 24449u);
}

TEST_CASE("npc_loot: missing name or template still consumes chance and quantity draws") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 1.0;
    table.chance_decrease = 1.0;
    table.quantity_decrease = 1.0;
    table.items = {
        make_item("NOPE", 0, 100, 100, 1, 3),
        make_item("UNKNOWN_IN_INDEX", 0, 100, 100, 1, 3),
    };

    MsvcRandom rng(9);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    CHECK(loot.hot.empty());
    CHECK(loot.main.empty());
    CHECK(rng.next() == 8047u);
}

TEST_CASE("npc_loot: no four-stack cap") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 1.0;
    table.chance_decrease = 1.0;
    table.quantity_decrease = 1.0;
    table.items = {make_item("E", 0, 100, 100, 10, 10)};

    MsvcRandom rng(1);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    const std::vector<LootItem> ten_units(10, LootItem{105, 1, 5, 0, 2});
    check_items(loot.hot, ten_units);
    check_items(loot.main, ten_units);
    CHECK(rng.next() == 26500u);
}

TEST_CASE("npc_loot: scaled bounds use trunc plus one when fractional") {
    const auto names = make_names();
    gl::assets::InvTable table;
    table.regenerate_chance = 1.0;
    table.chance_decrease = 1.0;
    table.quantity_decrease = 3.0;
    table.items = {make_item("F", 0, 100, 100, 4, 7)};

    MsvcRandom rng(13);
    const NpcLoot loot = NpcLootGenerator::generate(table, 11, names, lookup, rng);

    check_items(loot.hot, {{106, 2, 6, 0x84u, 9}, {106, 2, 6, 0x84u, 9}, {106, 2, 6, 0x84u, 9}});
    check_items(loot.main, {{106, 2, 6, 0x84u, 9}});
    CHECK(rng.next() == 30076u);
}

TEST_CASE("npc_loot: zero divisor guards skip pass 2") {
    const auto names = make_names();
    gl::assets::InvItem item = make_item("D", 0, 100, 100, 1, 1);

    gl::assets::InvTable zero_quantity;
    zero_quantity.regenerate_chance = 1.0;
    zero_quantity.chance_decrease = 1.0;
    zero_quantity.quantity_decrease = 0.0;
    zero_quantity.items = {item};

    MsvcRandom quantity_rng(1);
    const NpcLoot quantity_loot =
        NpcLootGenerator::generate(zero_quantity, 11, names, lookup, quantity_rng);
    check_items(quantity_loot.hot, {{104, 1, 4, 0, 1}});
    CHECK(quantity_loot.main.empty());
    CHECK(quantity_rng.next() == 6334u);

    gl::assets::InvTable zero_chance;
    zero_chance.regenerate_chance = 1.0;
    zero_chance.chance_decrease = 0.0;
    zero_chance.quantity_decrease = 1.0;
    zero_chance.items = {item};

    MsvcRandom chance_rng(1);
    const NpcLoot chance_loot =
        NpcLootGenerator::generate(zero_chance, 11, names, lookup, chance_rng);
    check_items(chance_loot.hot, {{104, 1, 4, 0, 1}});
    CHECK(chance_loot.main.empty());
    CHECK(chance_rng.next() == 6334u);
}
