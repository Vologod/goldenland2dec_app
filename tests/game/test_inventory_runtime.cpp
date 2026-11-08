#include "gl/game/inventory_runtime.hpp"

#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/sdb.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

using namespace gl::game;
using gl::assets::InventoryItem;

namespace {
std::optional<ItemTemplate> template_for(std::uint32_t item_id) {
    switch (item_id) {
    case 812:
        return ItemTemplate{18, 0xC0000000u, 1, 100000000u};
    case 1101:
        return ItemTemplate{26, 0, 0, 10000u};
    case 307:
        return ItemTemplate{12, 0xC0000084u, 8, 1u};
    default:
        return std::nullopt;
    }
}

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
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    auto access = runtime.access();
    REQUIRE(access.item_count);
    REQUIRE(access.money);
    REQUIRE(access.add_item);
    REQUIRE(access.remove_item);

    CHECK(access.item_count("Hero", "MON_1_0_1") == 500);
    CHECK(access.item_count("Other", "MON_1_0_1") == 0);
    CHECK(access.money() == 500);

    access.add_item("Hero", "MON_1_0_1", 100);
    CHECK(access.item_count("Hero", "MON_1_0_1") == 600);
    CHECK(access.money() == 600);
    access.remove_item("Hero", "MON_1_0_1", 100);
    CHECK(access.item_count("Hero", "MON_1_0_1") == 500);
    CHECK(access.money() == 500);
}

TEST_CASE("inventory_runtime mutations persist across access calls") {
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    auto first = runtime.access();
    first.add_item("Hero", "QST_1_0_159", 3);

    auto second = runtime.access();
    CHECK(second.item_count("Hero", "QST_1_0_159") == 3);
    CHECK(runtime.hero_inventory().items.size() == 1);

    second.remove_item("Hero", "QST_1_0_159", 3);
    auto third = runtime.access();
    CHECK(third.item_count("Hero", "QST_1_0_159") == 0);
    CHECK(runtime.hero_inventory().items.empty());
}

TEST_CASE("inventory_runtime adds a new item using its template") {
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    REQUIRE(runtime.add_item("Hero", "QST_1_0_159", 3));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 3);
    const auto &inventory = runtime.hero_inventory();
    REQUIRE(inventory.items.size() == 1);
    CHECK(inventory.items[0].item_id == 1101);
    CHECK(inventory.items[0].count == 3);
    CHECK(inventory.items[0].type == 26);
    CHECK(inventory.items[0].flags == 0);
    CHECK(inventory.items[0].style == 0);
}

TEST_CASE("inventory_runtime stacks into an existing item record") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 2, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);

    REQUIRE(runtime.add_item("Hero", "QST_1_0_159", 3));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 5);
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].count == 5);
}

TEST_CASE("inventory_runtime add money updates count and money") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 500, 0, 18, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);

    REQUIRE(runtime.add_item("Hero", "MON_1_0_1", 100));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 600);
    CHECK(runtime.money() == 600);
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].count == 600);
}

TEST_CASE("inventory_runtime add money creates a record when missing") {
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    REQUIRE(runtime.add_item("Hero", "MON_1_0_1", 100));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 100);
    CHECK(runtime.money() == 100);
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].type == 18);
}

TEST_CASE("inventory_runtime removes part of a stack") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 5, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);

    REQUIRE(runtime.remove_item("Hero", "QST_1_0_159", 2));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 3);
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].count == 3);
}

TEST_CASE("inventory_runtime removes a whole record") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 5, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);

    REQUIRE(runtime.remove_item("Hero", "QST_1_0_159", 5));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 0);
    CHECK(runtime.hero_inventory().items.empty());
}

TEST_CASE("inventory_runtime removes all records when count exceeds available") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 2, 0, 0, 0});
    inventory.items.push_back({1101, 3, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);

    REQUIRE(runtime.remove_item("Hero", "QST_1_0_159", 8));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 0);
    CHECK(runtime.hero_inventory().items.empty());
}

TEST_CASE("inventory_runtime removes across records in slot order") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 2, 0, 0, 0});
    inventory.items.push_back({1101, 3, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);

    REQUIRE(runtime.remove_item("Hero", "QST_1_0_159", 4));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 1);
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].count == 1);
}

TEST_CASE("inventory_runtime remove missing item is a no-op") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 5, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);

    CHECK_FALSE(runtime.remove_item("Hero", "MON_1_0_1", 1));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 5);
    CHECK(runtime.money() == 0);
}

TEST_CASE("inventory_runtime zero and negative counts are no-ops") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 5, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);

    CHECK_FALSE(runtime.add_item("Hero", "QST_1_0_159", 0));
    CHECK_FALSE(runtime.add_item("Hero", "QST_1_0_159", -2));
    CHECK_FALSE(runtime.remove_item("Hero", "QST_1_0_159", 0));
    CHECK_FALSE(runtime.remove_item("Hero", "QST_1_0_159", -2));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 5);
}

TEST_CASE("inventory_runtime unknown tech and person are no-ops") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);

    CHECK_FALSE(runtime.add_item("Hero", "NOPE", 1));
    CHECK_FALSE(runtime.remove_item("Hero", "NOPE", 1));
    CHECK_FALSE(runtime.add_item("Other", "QST_1_0_159", 1));
    CHECK_FALSE(runtime.remove_item("Other", "QST_1_0_159", 1));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 2);
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
}

TEST_CASE("inventory_runtime missing template makes add a no-op") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    CHECK_FALSE(runtime.add_item("Hero", "QST_1_0_159", 1));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 2);

    auto missing = [](std::uint32_t) -> std::optional<ItemTemplate> {
        return std::nullopt;
    };
    auto runtime2 = InventoryRuntime::create(make_inventory(), make_names(), missing);
    CHECK_FALSE(runtime2.add_item("Hero", "QST_1_0_159", 1));
    CHECK(runtime2.count("Hero", "QST_1_0_159") == 2);
}

TEST_CASE("inventory_runtime remove does not require a template") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 2, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names());

    REQUIRE(runtime.remove_item("Hero", "QST_1_0_159", 1));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 1);
}

TEST_CASE("inventory_runtime add splits new records by max_stack") {
    auto lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 1101)
            return ItemTemplate{26, 0, 0, 3};
        return std::nullopt;
    };
    auto runtime = InventoryRuntime::create({}, make_names(), lookup);

    REQUIRE(runtime.add_item("Hero", "QST_1_0_159", 7));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 7);
    const auto &items = runtime.hero_inventory().items;
    REQUIRE(items.size() == 3);
    CHECK(items[0].count == 3);
    CHECK(items[1].count == 3);
    CHECK(items[2].count == 1);
}

TEST_CASE("inventory_runtime add fills an existing stack before splitting") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 2, 0, 0, 0});
    auto lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 1101)
            return ItemTemplate{26, 0, 0, 3};
        return std::nullopt;
    };
    auto runtime = InventoryRuntime::create(inventory, make_names(), lookup);

    REQUIRE(runtime.add_item("Hero", "QST_1_0_159", 4));
    const auto &items = runtime.hero_inventory().items;
    REQUIRE(items.size() == 2);
    CHECK(items[0].count == 3);
    CHECK(items[1].count == 3);
}

TEST_CASE("inventory_runtime add starts a new record when the stack is full") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 3, 0, 0, 0});
    auto lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 1101)
            return ItemTemplate{26, 0, 0, 3};
        return std::nullopt;
    };
    auto runtime = InventoryRuntime::create(inventory, make_names(), lookup);

    REQUIRE(runtime.add_item("Hero", "QST_1_0_159", 1));
    const auto &items = runtime.hero_inventory().items;
    REQUIRE(items.size() == 2);
    CHECK(items[0].count == 3);
    CHECK(items[1].count == 1);
}

TEST_CASE("inventory_runtime max_stack one creates one record per unit") {
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    REQUIRE(runtime.add_item("Hero", "ARM_1_08_2", 3));
    const auto &items = runtime.hero_inventory().items;
    REQUIRE(items.size() == 3);
    CHECK(items[0].count == 1);
    CHECK(items[1].count == 1);
    CHECK(items[2].count == 1);
    CHECK(items[0].type == 12);
    CHECK(items[0].style == 8);
}

TEST_CASE("inventory_runtime max_stack zero makes add a no-op") {
    auto lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 1101)
            return ItemTemplate{26, 0, 0, 0};
        return std::nullopt;
    };
    auto runtime = InventoryRuntime::create({}, make_names(), lookup);

    CHECK_FALSE(runtime.add_item("Hero", "QST_1_0_159", 1));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 0);
    CHECK(runtime.hero_inventory().items.empty());
}

TEST_CASE("inventory_runtime hero_inventory reflects mutations") {
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    REQUIRE(runtime.add_item("Hero", "QST_1_0_159", 2));
    REQUIRE(runtime.add_item("Hero", "ARM_1_08_2", 1));
    REQUIRE(runtime.hero_inventory().items.size() == 2);
    REQUIRE(runtime.remove_item("Hero", "QST_1_0_159", 2));
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].item_id == 307);
}

TEST_CASE("inventory_runtime access callbacks observe mutations") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    auto access = runtime.access();

    REQUIRE(runtime.add_item("Hero", "MON_1_0_1", 100));
    CHECK(access.item_count("Hero", "MON_1_0_1") == 600);
    CHECK(access.money() == 600);

    REQUIRE(runtime.remove_item("Hero", "MON_1_0_1", 100));
    CHECK(access.item_count("Hero", "MON_1_0_1") == 500);
    CHECK(access.money() == 500);
}

TEST_CASE("inventory_runtime stores npc main containers by exact name") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    runtime.set_person_items("L0.P1",
                             {InventoryItem{1101, 4, 0, 0, 0}, InventoryItem{812, 250, 0, 18, 0}});

    CHECK(runtime.count("L0.P1", "QST_1_0_159") == 4);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 250);
    CHECK(runtime.count("l0.p1", "QST_1_0_159") == 0);
    CHECK(runtime.count("L0.P2", "QST_1_0_159") == 0);
    REQUIRE(runtime.person_items("L0.P1").size() == 2);
    CHECK(runtime.person_items("L0.P1")[0].item_id == 1101);
    CHECK(runtime.person_items("L0.P2").empty());
}

TEST_CASE("inventory_runtime keeps the first npc container for duplicate names") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    runtime.set_person_items("L0.P1", {InventoryItem{1101, 4, 0, 0, 0}});
    runtime.set_person_items("L0.P1", {InventoryItem{812, 250, 0, 18, 0}});

    CHECK(runtime.count("L0.P1", "QST_1_0_159") == 4);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 0);
    REQUIRE(runtime.person_items("L0.P1").size() == 1);
    CHECK(runtime.person_items("L0.P1")[0].item_id == 1101);
}

TEST_CASE("inventory_runtime npc money does not change hero money") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    runtime.set_person_items("L0.P1", {InventoryItem{812, 250, 0, 18, 0}});

    CHECK(runtime.money() == 500);
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
}

TEST_CASE("inventory_runtime ignores set_person_items for Hero") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    runtime.set_person_items("Hero", {InventoryItem{1101, 99, 0, 0, 0}});

    CHECK(runtime.count("Hero", "QST_1_0_159") == 2);
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    REQUIRE(runtime.hero_inventory().items.size() == 3);
    CHECK(runtime.person_items("Hero").size() == 3);
}

TEST_CASE("inventory_runtime npc add and remove remain no-ops") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {InventoryItem{1101, 4, 0, 0, 0}});

    CHECK_FALSE(runtime.add_item("L0.P1", "QST_1_0_159", 1));
    CHECK_FALSE(runtime.remove_item("L0.P1", "QST_1_0_159", 1));
    CHECK(runtime.count("L0.P1", "QST_1_0_159") == 4);

    REQUIRE(runtime.add_item("Hero", "QST_1_0_159", 1));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 3);
}

TEST_CASE("inventory_runtime clear_persons keeps Hero") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());
    runtime.set_person_items("L0.P1", {InventoryItem{1101, 4, 0, 0, 0}});
    runtime.set_person_items("L0.P2", {InventoryItem{812, 10, 0, 18, 0}});

    runtime.clear_persons();

    CHECK(runtime.count("L0.P1", "QST_1_0_159") == 0);
    CHECK(runtime.count("L0.P2", "MON_1_0_1") == 0);
    CHECK(runtime.person_items("L0.P1").empty());
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.money() == 500);
}

TEST_CASE("inventory_runtime transfer_item moves part of a stack to an npc") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    REQUIRE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 100));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 400);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 100);
    CHECK(runtime.hero_inventory().items.size() == 3);
}

TEST_CASE("inventory_runtime transfer_item moves an exact stack and removes the source record") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    REQUIRE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 500));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 0);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 500);
    REQUIRE(runtime.hero_inventory().items.size() == 2);
    CHECK(runtime.hero_inventory().items[0].item_id == 1101);
}

TEST_CASE("inventory_runtime transfer_item over-move takes everything and returns true") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    REQUIRE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 600));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 0);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 500);
    REQUIRE(runtime.hero_inventory().items.size() == 2);
}

TEST_CASE("inventory_runtime transfer_item absent item matrix") {
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    CHECK_FALSE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 1));
    CHECK(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 0));
    CHECK(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", -1));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 0);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 0);

    CHECK_FALSE(runtime.transfer_item("Hero", "L0.P1", "NOPE", 1));
    CHECK(runtime.transfer_item("Hero", "L0.P1", "NOPE", 0));
    CHECK(runtime.transfer_item("Hero", "L0.P1", "NOPE", -1));
}

TEST_CASE("inventory_runtime transfer_item same person is a no-op true") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);

    CHECK(runtime.transfer_item("Hero", "Hero", "MON_1_0_1", 100));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.transfer_item("L0.Missing", "L0.Missing", "MON_1_0_1", 100));
}

TEST_CASE("inventory_runtime transfer_item merges into an existing dest stack") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {InventoryItem{812, 50, 0x1234u, 7, 9}});

    REQUIRE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 100));
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 150);
    REQUIRE(runtime.person_items("L0.P1").size() == 1);
    CHECK(runtime.person_items("L0.P1")[0].flags == 0x1234u);
    CHECK(runtime.person_items("L0.P1")[0].type == 7);
    CHECK(runtime.person_items("L0.P1")[0].style == 9);
}

TEST_CASE("inventory_runtime transfer_item new dest record uses the item template") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 500, 0x8000020Cu, 99, 77});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    REQUIRE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 10));
    REQUIRE(runtime.person_items("L0.P1").size() == 1);
    const InventoryItem &item = runtime.person_items("L0.P1")[0];
    CHECK(item.item_id == 812);
    CHECK(item.count == 10);
    CHECK(item.type == 18);
    CHECK(item.flags == 0xC0000000u);
    CHECK(item.style == 1);
}

TEST_CASE("inventory_runtime transfer_item walks duplicate records in slot order") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 2, 0, 0, 0});
    inventory.items.push_back({1101, 3, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    REQUIRE(runtime.transfer_item("Hero", "L0.P1", "QST_1_0_159", 4));
    CHECK(runtime.count("Hero", "QST_1_0_159") == 1);
    CHECK(runtime.count("L0.P1", "QST_1_0_159") == 4);
    REQUIRE(runtime.hero_inventory().items.size() == 1);
    CHECK(runtime.hero_inventory().items[0].count == 1);
}

TEST_CASE("inventory_runtime transfer_item negative count is limited to one million") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 1000001, 0, 18, 1});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    REQUIRE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", -1));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 1);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 1000000);
}

TEST_CASE("inventory_runtime transfer_item moves from npc to hero") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {InventoryItem{812, 100, 0, 18, 0}});

    REQUIRE(runtime.transfer_item("L0.P1", "Hero", "MON_1_0_1", 40));
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 60);
    CHECK(runtime.count("Hero", "MON_1_0_1") == 540);
    CHECK(runtime.money() == 540);
}

TEST_CASE("inventory_runtime transfer_item missing person is false and unchanged") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);

    CHECK_FALSE(runtime.transfer_item("Hero", "L0.Missing", "MON_1_0_1", 100));
    CHECK_FALSE(runtime.transfer_item("L0.Missing", "Hero", "MON_1_0_1", 100));
    CHECK_FALSE(runtime.transfer_item("Hero", "L0.Missing", "MON_1_0_1", 0));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.person_items("L0.Missing").empty());
}

TEST_CASE("inventory_runtime transfer_item missing template keeps both sides unchanged") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());
    runtime.set_person_items("L0.P1", {});

    CHECK_FALSE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 100));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 0);
}

TEST_CASE("inventory_runtime transfer_all moves every source record and clears source") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {InventoryItem{1101, 1, 0, 0, 0}});

    REQUIRE(runtime.transfer_all("Hero", "L0.P1"));
    CHECK(runtime.hero_inventory().items.empty());
    CHECK(runtime.money() == 0);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 500);
    CHECK(runtime.count("L0.P1", "QST_1_0_159") == 3);
    CHECK(runtime.count("L0.P1", "ARM_1_08_2") == 1);
}

TEST_CASE("inventory_runtime transfer_all empty source is true") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    REQUIRE(runtime.transfer_all("L0.P1", "Hero"));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 0);
}

TEST_CASE("inventory_runtime transfer_all merges equal item ids") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {InventoryItem{812, 50, 0, 18, 0}});

    REQUIRE(runtime.transfer_all("Hero", "L0.P1"));
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 550);
    CHECK(runtime.count("L0.P1", "QST_1_0_159") == 2);
    CHECK(runtime.count("L0.P1", "ARM_1_08_2") == 1);
    CHECK(runtime.person_items("L0.P1").size() == 3);
}

TEST_CASE("inventory_runtime transfer_all splits by template max_stack") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 7, 0, 0, 0});
    auto lookup = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 1101)
            return ItemTemplate{26, 0, 0, 3};
        return std::nullopt;
    };
    auto runtime = InventoryRuntime::create(inventory, make_names(), lookup);
    runtime.set_person_items("L0.P1", {});

    REQUIRE(runtime.transfer_all("Hero", "L0.P1"));
    const auto &items = runtime.person_items("L0.P1");
    REQUIRE(items.size() == 3);
    CHECK(items[0].count == 3);
    CHECK(items[1].count == 3);
    CHECK(items[2].count == 1);
}

TEST_CASE("inventory_runtime transfer_all same person is a no-op true") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);

    CHECK(runtime.transfer_all("Hero", "Hero"));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.hero_inventory().items.size() == 3);
}

TEST_CASE("inventory_runtime transfer_all missing person is false and unchanged") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);

    CHECK_FALSE(runtime.transfer_all("Hero", "L0.Missing"));
    CHECK_FALSE(runtime.transfer_all("L0.Missing", "Hero"));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.hero_inventory().items.size() == 3);
}

TEST_CASE("inventory_runtime access exposes transfer callbacks") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names(), template_for);
    runtime.set_person_items("L0.P1", {});
    auto access = runtime.access();
    REQUIRE(access.transfer_item);
    REQUIRE(access.transfer_all);

    CHECK(access.transfer_item("Hero", "L0.P1", "MON_1_0_1", 100));
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 100);
    CHECK(access.transfer_all("Hero", "L0.P1"));
    CHECK(runtime.hero_inventory().items.empty());
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 500);
}
