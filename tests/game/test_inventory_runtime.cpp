#include "gl/game/inventory_runtime.hpp"

#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/sdb.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string_view>
#include <vector>

using namespace gl::game;

namespace {
gl::assets::ItemNameIndex make_names() {
    const std::vector<gl::assets::SdbEntry> entries = {
        {812, "MON_1_0_1"},
        {1101, "QST_1_0_159"},
        {307, "ARM_1_08_2"},
    };
    auto index = gl::assets::ItemNameIndex::from_entries(entries);
    REQUIRE(index.has_value());
    return index.value();
}

gl::assets::HeroInventory make_inventory() {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 500, 0, 18, 0});
    inventory.items.push_back({1101, 2, 0, 0, 0});
    inventory.items.push_back({307, 1, 0, 0, 0});
    return inventory;
}
} // namespace

TEST_CASE("inventory_runtime counts hero items by tech name") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.count("Hero", "QST_1_0_159") == 2);
    CHECK(runtime.count("Hero", "ARM_1_08_2") == 1);
}

TEST_CASE("inventory_runtime returns zero for unknown tech and non-hero person") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    CHECK(runtime.count("Hero", "NOPE") == 0);
    CHECK(runtime.count("Hero", "mon_1_0_1") == 0);
    CHECK(runtime.count("Other", "MON_1_0_1") == 0);
    CHECK(runtime.count("", "MON_1_0_1") == 0);
}

TEST_CASE("inventory_runtime money sums type 18 counts") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 500, 0, 18, 0});
    inventory.items.push_back({813, 25, 0, 18, 0});
    inventory.items.push_back({1101, 2, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names());

    CHECK(runtime.money() == 525);
}

TEST_CASE("inventory_runtime sums counts for duplicate item ids") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 2, 0, 0, 0});
    inventory.items.push_back({1101, 3, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names());

    CHECK(runtime.count("Hero", "QST_1_0_159") == 5);
}

TEST_CASE("inventory_runtime empty inventory returns zero") {
    auto runtime = InventoryRuntime::create({}, make_names());

    CHECK(runtime.count("Hero", "MON_1_0_1") == 0);
    CHECK(runtime.money() == 0);
}

TEST_CASE("inventory_runtime access callbacks delegate to queries") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());
    auto access = runtime.access();
    REQUIRE(access.item_count);
    REQUIRE(access.money);

    CHECK(access.item_count("Hero", "MON_1_0_1") == 500);
    CHECK(access.item_count("Other", "MON_1_0_1") == 0);
    CHECK(access.money() == 500);
}
