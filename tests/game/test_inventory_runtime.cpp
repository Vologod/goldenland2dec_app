#include "gl/game/inventory_runtime.hpp"

#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/inv_table.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/npc_loot.hpp"
#include "gl/game/person.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
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

constexpr std::string_view kTradeNpc = "L13_1.P517_Boat_Trader";

std::optional<ItemTemplate> trade_template_for(std::uint32_t item_id) {
    switch (item_id) {
    case 812:
        return ItemTemplate{18, 0xC0000000u, 1, 100000000u};
    case 953:
        return ItemTemplate{19, 0x8000000Cu, 0, 10000u};
    default:
        return std::nullopt;
    }
}

gl::assets::ItemNameIndex make_trade_names() {
    const std::vector<gl::assets::SdbEntry> entries = {
        {812, "MON_1_0_1"},
        {953, "POT_1_00_27"},
    };
    auto index = gl::assets::ItemNameIndex::from_entries(entries);
    REQUIRE(index.has_value());
    return index.value();
}

InventoryRuntime make_trade_runtime(std::uint32_t hero_money, std::uint32_t hero_potion,
                                    std::uint32_t npc_money, std::uint32_t npc_potion) {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, hero_money, 0, 18, 0});
    if (hero_potion != 0)
        inventory.items.push_back({953, hero_potion, 0x8000000Cu, 19, 0});
    auto runtime = InventoryRuntime::create(inventory, make_trade_names(), trade_template_for);
    runtime.set_person_items(std::string(kTradeNpc), {});
    std::vector<InventoryItem> trade;
    trade.push_back({812, npc_money, 0, 18, 0});
    if (npc_potion != 0)
        trade.push_back({953, npc_potion, 0x8000000Cu, 19, 0});
    runtime.set_person_trade_items(std::string(kTradeNpc), std::move(trade));
    return runtime;
}

TradePrice whole_stack_price(std::uint32_t skill, std::uint32_t count) {
    TradePriceInput input{};
    input.item_id = 953;
    input.item_price = 500;
    input.durability = 100;
    input.flags = 0x8000000Cu;
    input.mod_count = 0;
    input.skill_trade = skill;
    input.count = count;
    input.assume_pc53 = true;
    return compute_trade_price(input).value();
}

std::int64_t trade_item_count(const InventoryRuntime &runtime, std::uint32_t item_id) {
    std::int64_t total = 0;
    for (const InventoryItem &item : runtime.trade_items(kTradeNpc)) {
        if (item.item_id == item_id)
            total += item.count;
    }
    return total;
}

std::int64_t main_item_count(const InventoryRuntime &runtime, std::string_view person,
                             std::uint32_t item_id) {
    std::int64_t total = 0;
    for (const InventoryItem &item : runtime.person_items(person)) {
        if (item.item_id == item_id)
            total += item.count;
    }
    return total;
}

void check_items_equal(const std::vector<InventoryItem> &actual,
                       const std::vector<InventoryItem> &expected) {
    REQUIRE(actual.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CHECK(actual[index].item_id == expected[index].item_id);
        CHECK(actual[index].count == expected[index].count);
        CHECK(actual[index].flags == expected[index].flags);
        CHECK(actual[index].type == expected[index].type);
        CHECK(actual[index].style == expected[index].style);
    }
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

    auto missing = [](std::uint32_t) -> std::optional<ItemTemplate> { return std::nullopt; };
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
    CHECK_FALSE(runtime.transfer_item("L0.Missing", "L0.Missing", "MON_1_0_1", 100));
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
    CHECK_FALSE(runtime.transfer_item("L0.Missing", "L0.Missing", "MON_1_0_1", 100));
    CHECK_FALSE(runtime.transfer_all("L0.Missing", "L0.Missing"));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.person_items("L0.Missing").empty());
}

TEST_CASE("inventory_runtime transfer_item missing template keeps both sides unchanged") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());
    runtime.set_person_items("L0.P1", {});

    CHECK_FALSE(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 100));
    CHECK(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", 0));
    CHECK(runtime.transfer_item("Hero", "L0.P1", "MON_1_0_1", -1));
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 0);
}

TEST_CASE("inventory_runtime transfer_all aborts when a source record has no usable template") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({1101, 2, 0, 0, 0});
    inventory.items.push_back({999, 1, 0, 0, 0});
    auto runtime = InventoryRuntime::create(inventory, make_names(), template_for);
    runtime.set_person_items("L0.P1", {});

    CHECK_FALSE(runtime.transfer_all("Hero", "L0.P1"));
    CHECK(runtime.hero_inventory().items.size() == 2);
    CHECK(runtime.count("Hero", "QST_1_0_159") == 2);
    CHECK(runtime.person_items("L0.P1").empty());

    auto zero_stack = [](std::uint32_t item_id) -> std::optional<ItemTemplate> {
        if (item_id == 1101)
            return ItemTemplate{26, 0, 0, 0};
        return std::nullopt;
    };
    gl::assets::HeroInventory other;
    other.items.push_back({1101, 2, 0, 0, 0});
    auto runtime2 = InventoryRuntime::create(other, make_names(), zero_stack);
    runtime2.set_person_items("L0.P2", {});
    CHECK_FALSE(runtime2.transfer_all("Hero", "L0.P2"));
    CHECK(runtime2.hero_inventory().items.size() == 1);
    CHECK(runtime2.person_items("L0.P2").empty());
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

TEST_CASE("inventory_runtime retains npc trade items separately from main") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    runtime.set_person_items("L0.P1", {InventoryItem{1101, 4, 0, 0, 0}});
    runtime.set_person_trade_items("L0.P1", {InventoryItem{812, 42142, 0, 18, 0}});

    CHECK(runtime.count("L0.P1", "QST_1_0_159") == 4);
    CHECK(runtime.count("L0.P1", "MON_1_0_1") == 0);
    CHECK(runtime.trade_money("L0.P1") == 42142);
    REQUIRE(runtime.trade_items("L0.P1").size() == 1);
    CHECK(runtime.trade_items("L0.P1")[0].item_id == 812);
    CHECK(runtime.trade_items("L0.P1")[0].count == 42142);
}

TEST_CASE("inventory_runtime trade money sums type 18 trade records") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    runtime.set_person_trade_items("L0.P1", {InventoryItem{812, 12266, 0, 18, 0},
                                             InventoryItem{813, 100, 0, 18, 0},
                                             InventoryItem{408, 10, 0, 20, 0}});

    CHECK(runtime.trade_money("L0.P1") == 12366);
    CHECK(runtime.trade_items("L0.P1").size() == 3);
}

TEST_CASE("inventory_runtime keeps the first trade container for duplicate names") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    runtime.set_person_trade_items("L0.P1", {InventoryItem{408, 10, 0, 20, 0}});
    runtime.set_person_trade_items("L0.P1", {InventoryItem{812, 250, 0, 18, 0}});

    CHECK(runtime.trade_money("L0.P1") == 0);
    REQUIRE(runtime.trade_items("L0.P1").size() == 1);
    CHECK(runtime.trade_items("L0.P1")[0].item_id == 408);
}

TEST_CASE("inventory_runtime hero trade is empty until set") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());

    CHECK(runtime.trade_items("Hero").empty());
    CHECK(runtime.trade_money("Hero") == 0);
    CHECK(runtime.trade_items("L0.Missing").empty());
    CHECK(runtime.trade_money("L0.Missing") == 0);

    runtime.set_person_trade_items("Hero", {InventoryItem{812, 50, 0, 18, 0}});
    CHECK(runtime.trade_money("Hero") == 50);
    CHECK(runtime.money() == 500);
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
}

TEST_CASE("inventory_runtime clear_persons clears trade containers") {
    auto runtime = InventoryRuntime::create(make_inventory(), make_names());
    runtime.set_person_items("L0.P1", {InventoryItem{1101, 4, 0, 0, 0}});
    runtime.set_person_trade_items("L0.P1", {InventoryItem{812, 10, 0, 18, 0}});

    runtime.clear_persons();

    CHECK(runtime.trade_items("L0.P1").empty());
    CHECK(runtime.trade_money("L0.P1") == 0);
    CHECK(runtime.count("Hero", "MON_1_0_1") == 500);
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

TEST_CASE("generate_npc_inventories stores pass-1 hot as npc trade items") {
    struct ScopedDir {
        std::filesystem::path path;
        ~ScopedDir() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    };
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    ScopedDir dir{std::filesystem::temp_directory_path() /
                  ("gl_trade_inv_" + std::to_string(stamp))};
    const std::filesystem::path inv_path = dir.path / "scripts" / "inventory" / "trade_test.inv";
    std::filesystem::create_directories(inv_path.parent_path());
    {
        std::ofstream out(inv_path, std::ios::binary);
        REQUIRE(out.good());
        out << "regenerate_chance 100\n"
               "chance_decrease 1\n"
               "quantity_decrease 1\n"
               "item \"MON_1_0_1\" 0 100 100 7 7\n";
    }

    auto vfs = gl::assets::GameVfs::mount(dir.path);
    REQUIRE(vfs.has_value());
    auto runtime = InventoryRuntime::create({}, make_names(), template_for);

    PersonInstance person;
    person.name = "L0.Trade";
    person.inventory = "trade_test";
    MsvcRandom rng(1);
    generate_npc_inventories(runtime, std::vector<PersonInstance>{person}, vfs.value(),
                             make_names(), template_for, 50, rng);

    REQUIRE_FALSE(runtime.trade_items("L0.Trade").empty());
    CHECK(runtime.trade_money("L0.Trade") == 7);
    REQUIRE(runtime.trade_items("L0.Trade").size() == 1);
    CHECK(runtime.trade_items("L0.Trade")[0].item_id == 812);
    CHECK(runtime.trade_items("L0.Trade")[0].count == 7);
    CHECK(runtime.count("L0.Trade", "MON_1_0_1") == 7);
    CHECK(runtime.money() == 0);
}

TEST_CASE("inventory_runtime buy_trade moves the whole npc stack and hero money") {
    auto runtime = make_trade_runtime(43875, 51, 42142, 14);
    const TradePrice price = whole_stack_price(1, 14);
    REQUIRE(price.buy == 16800);

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", price) == TradeStatus::ok);

    CHECK(runtime.count("Hero", "POT_1_00_27") == 65);
    CHECK(runtime.count("Hero", "MON_1_0_1") == 27075);
    CHECK(runtime.money() == 27075);
    CHECK(trade_item_count(runtime, 953) == 0);
    CHECK(runtime.trade_money(kTradeNpc) == 58942);
    REQUIRE(runtime.person_items("Hero").size() == 2);
    CHECK(main_item_count(runtime, "Hero", 953) == 65);
    CHECK(main_item_count(runtime, "Hero", 953) + trade_item_count(runtime, 953) == 65);
    CHECK(runtime.money() + runtime.trade_money(kTradeNpc) == 86017);
}

TEST_CASE("inventory_runtime buy_trade creates a new hero record from the item template") {
    auto runtime = make_trade_runtime(43875, 0, 42142, 14);

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::ok);

    const auto &hero = runtime.person_items("Hero");
    REQUIRE(hero.size() == 2);
    CHECK(hero[1].item_id == 953);
    CHECK(hero[1].count == 14);
    CHECK(hero[1].type == 19);
    CHECK(hero[1].flags == 0x8000000Cu);
    CHECK(hero[1].style == 0);
}

TEST_CASE("inventory_runtime buy_trade merges into a hero stack and keeps its flags") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 43875, 0, 18, 0});
    inventory.items.push_back({953, 51, 0x12345678u, 19, 0});
    auto runtime2 = InventoryRuntime::create(inventory, make_trade_names(), trade_template_for);
    runtime2.set_person_items(std::string(kTradeNpc), {});
    runtime2.set_person_trade_items(
        std::string(kTradeNpc),
        {InventoryItem{812, 42142, 0, 18, 0}, InventoryItem{953, 14, 0x8000000Cu, 19, 0}});

    CHECK(runtime2.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::ok);
    REQUIRE(runtime2.person_items("Hero").size() == 2);
    CHECK(runtime2.person_items("Hero")[1].count == 65);
    CHECK(runtime2.person_items("Hero")[1].flags == 0x12345678u);
}

TEST_CASE("inventory_runtime sell_trade moves the whole hero stack and npc money") {
    auto runtime = make_trade_runtime(43875, 51, 42142, 14);
    const TradePrice price = whole_stack_price(1, 51);
    REQUIRE(price.sell == 10608);

    CHECK(runtime.sell_trade(kTradeNpc, "Hero", "POT_1_00_27", price) == TradeStatus::ok);

    CHECK(runtime.count("Hero", "POT_1_00_27") == 0);
    CHECK(runtime.count("Hero", "MON_1_0_1") == 54483);
    CHECK(runtime.money() == 54483);
    CHECK(trade_item_count(runtime, 953) == 65);
    CHECK(runtime.trade_money(kTradeNpc) == 31534);
    CHECK(main_item_count(runtime, "Hero", 953) + trade_item_count(runtime, 953) == 65);
    CHECK(runtime.money() + runtime.trade_money(kTradeNpc) == 86017);
    REQUIRE(runtime.trade_items(kTradeNpc).size() == 2);
}

TEST_CASE("inventory_runtime sell_trade creates an npc trade record from the item template") {
    auto runtime = make_trade_runtime(43875, 51, 42142, 0);

    CHECK(runtime.sell_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 51)) ==
          TradeStatus::ok);

    const auto &trade = runtime.trade_items(kTradeNpc);
    REQUIRE(trade.size() == 2);
    CHECK(trade[1].item_id == 953);
    CHECK(trade[1].count == 51);
    CHECK(trade[1].type == 19);
    CHECK(trade[1].flags == 0x8000000Cu);
    CHECK(trade[1].style == 0);
}

TEST_CASE("inventory_runtime buyer pays the whole-stack price") {
    auto runtime = make_trade_runtime(16800, 0, 42142, 14);

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::ok);
    CHECK(runtime.money() == 0);
    CHECK(runtime.trade_money(kTradeNpc) == 58942);
}

TEST_CASE("inventory_runtime buy_trade without enough hero money leaves state unchanged") {
    auto runtime = make_trade_runtime(1000, 51, 42142, 14);
    const auto hero_before = runtime.person_items("Hero");
    const auto trade_before = runtime.trade_items(kTradeNpc);

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::insufficient_money);

    check_items_equal(runtime.person_items("Hero"), hero_before);
    check_items_equal(runtime.trade_items(kTradeNpc), trade_before);
    CHECK(runtime.money() == 1000);
    CHECK(runtime.trade_money(kTradeNpc) == 42142);
}

TEST_CASE("inventory_runtime buy_trade without npc stock leaves state unchanged") {
    auto runtime = make_trade_runtime(43875, 51, 42142, 0);
    const auto hero_before = runtime.person_items("Hero");
    const auto trade_before = runtime.trade_items(kTradeNpc);

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 1)) ==
          TradeStatus::insufficient_stock);

    check_items_equal(runtime.person_items("Hero"), hero_before);
    check_items_equal(runtime.trade_items(kTradeNpc), trade_before);
    CHECK(runtime.money() == 43875);
    CHECK(runtime.trade_money(kTradeNpc) == 42142);
}

TEST_CASE("inventory_runtime buy_trade rejects a price computed for another stack") {
    auto runtime = make_trade_runtime(43875, 51, 42142, 14);
    const auto hero_before = runtime.person_items("Hero");
    const auto trade_before = runtime.trade_items(kTradeNpc);

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 13)) ==
          TradeStatus::unsupported_price);

    check_items_equal(runtime.person_items("Hero"), hero_before);
    check_items_equal(runtime.trade_items(kTradeNpc), trade_before);
}

TEST_CASE("inventory_runtime sell_trade without enough npc money leaves state unchanged") {
    auto runtime = make_trade_runtime(43875, 51, 100, 14);
    const auto hero_before = runtime.person_items("Hero");
    const auto trade_before = runtime.trade_items(kTradeNpc);

    CHECK(runtime.sell_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 51)) ==
          TradeStatus::insufficient_money);

    check_items_equal(runtime.person_items("Hero"), hero_before);
    check_items_equal(runtime.trade_items(kTradeNpc), trade_before);
    CHECK(runtime.money() == 43875);
    CHECK(runtime.trade_money(kTradeNpc) == 100);
}

TEST_CASE("inventory_runtime sell_trade without hero stock leaves state unchanged") {
    auto runtime = make_trade_runtime(43875, 0, 42142, 14);
    const auto hero_before = runtime.person_items("Hero");
    const auto trade_before = runtime.trade_items(kTradeNpc);

    CHECK(runtime.sell_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 1)) ==
          TradeStatus::insufficient_stock);

    check_items_equal(runtime.person_items("Hero"), hero_before);
    check_items_equal(runtime.trade_items(kTradeNpc), trade_before);
}

TEST_CASE("inventory_runtime trade rejects unsupported and zero prices without mutation") {
    auto runtime = make_trade_runtime(43875, 51, 42142, 14);
    const auto hero_before = runtime.person_items("Hero");
    const auto trade_before = runtime.trade_items(kTradeNpc);

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", TradePrice{}) ==
          TradeStatus::unsupported_price);
    CHECK(runtime.sell_trade(kTradeNpc, "Hero", "POT_1_00_27", TradePrice{}) ==
          TradeStatus::unsupported_price);

    check_items_equal(runtime.person_items("Hero"), hero_before);
    check_items_equal(runtime.trade_items(kTradeNpc), trade_before);
}

TEST_CASE("inventory_runtime trade rejects missing persons without mutation") {
    auto runtime = make_trade_runtime(43875, 51, 42142, 14);
    const auto hero_before = runtime.person_items("Hero");
    const auto trade_before = runtime.trade_items(kTradeNpc);

    CHECK(runtime.buy_trade("L9.Missing", "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::missing_person);
    CHECK(runtime.sell_trade("L9.Missing", "Hero", "POT_1_00_27", whole_stack_price(1, 51)) ==
          TradeStatus::missing_person);
    CHECK(runtime.buy_trade(kTradeNpc, "Other", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::missing_person);
    CHECK(runtime.sell_trade(kTradeNpc, "Other", "POT_1_00_27", whole_stack_price(1, 51)) ==
          TradeStatus::missing_person);

    check_items_equal(runtime.person_items("Hero"), hero_before);
    check_items_equal(runtime.trade_items(kTradeNpc), trade_before);
}

TEST_CASE("inventory_runtime trade without a template leaves state unchanged") {
    auto runtime = InventoryRuntime::create({}, make_trade_names());
    runtime.set_person_items(std::string(kTradeNpc), {});
    runtime.set_person_trade_items(
        std::string(kTradeNpc),
        {InventoryItem{812, 42142, 0, 18, 0}, InventoryItem{953, 14, 0x8000000Cu, 19, 0}});

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::missing_template);
    CHECK(runtime.money() == 0);
    CHECK(trade_item_count(runtime, 953) == 14);
}

TEST_CASE("inventory_runtime trade without a money template leaves state unchanged") {
    const std::vector<gl::assets::SdbEntry> entries = {{953, "POT_1_00_27"}};
    auto names = gl::assets::ItemNameIndex::from_entries(entries);
    REQUIRE(names.has_value());
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 43875, 0, 18, 0});
    inventory.items.push_back({953, 51, 0x8000000Cu, 19, 0});
    auto runtime = InventoryRuntime::create(inventory, names.value(), trade_template_for);
    runtime.set_person_items(std::string(kTradeNpc), {});
    runtime.set_person_trade_items(
        std::string(kTradeNpc),
        {InventoryItem{812, 42142, 0, 18, 0}, InventoryItem{953, 14, 0x8000000Cu, 19, 0}});

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::missing_template);
    CHECK(runtime.money() == 43875);
    CHECK(runtime.count("Hero", "POT_1_00_27") == 51);
    CHECK(trade_item_count(runtime, 953) == 14);
}

TEST_CASE("inventory_runtime sell_trade moves the first matching source record only") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 43875, 0, 18, 0});
    inventory.items.push_back({953, 20, 0x8000000Cu, 19, 0});
    inventory.items.push_back({953, 31, 0x8000000Cu, 19, 0});
    auto runtime = InventoryRuntime::create(inventory, make_trade_names(), trade_template_for);
    runtime.set_person_items(std::string(kTradeNpc), {});
    runtime.set_person_trade_items(std::string(kTradeNpc), {InventoryItem{812, 42142, 0, 18, 0}});

    CHECK(runtime.sell_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 20)) ==
          TradeStatus::ok);

    CHECK(main_item_count(runtime, "Hero", 953) == 31);
    CHECK(trade_item_count(runtime, 953) == 20);
    REQUIRE(runtime.person_items("Hero").size() == 2);
    CHECK(runtime.person_items("Hero")[1].count == 31);
}

TEST_CASE("inventory_runtime buy_trade copies the moved record metadata to a new destination") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 43875, 0, 18, 0});
    auto runtime = InventoryRuntime::create(inventory, make_trade_names(), trade_template_for);
    runtime.set_person_items(std::string(kTradeNpc), {});
    runtime.set_person_trade_items(
        std::string(kTradeNpc),
        {InventoryItem{812, 42142, 0, 18, 0}, InventoryItem{953, 14, 0xDEADBEEFu, 19, 3}});

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::ok);

    const auto &hero = runtime.person_items("Hero");
    REQUIRE(hero.size() == 2);
    CHECK(hero[1].item_id == 953);
    CHECK(hero[1].count == 14);
    CHECK(hero[1].type == 19);
    CHECK(hero[1].flags == 0xDEADBEEFu);
    CHECK(hero[1].style == 3);
}

TEST_CASE("inventory_runtime buy_trade removes money across split source records") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 20000, 0, 18, 0});
    inventory.items.push_back({812, 23875, 0, 18, 0});
    inventory.items.push_back({953, 51, 0x8000000Cu, 19, 0});
    auto runtime = InventoryRuntime::create(inventory, make_trade_names(), trade_template_for);
    runtime.set_person_items(std::string(kTradeNpc), {});
    runtime.set_person_trade_items(
        std::string(kTradeNpc),
        {InventoryItem{812, 42142, 0, 18, 0}, InventoryItem{953, 14, 0x8000000Cu, 19, 0}});

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 14)) ==
          TradeStatus::ok);

    const auto &hero = runtime.person_items("Hero");
    REQUIRE(hero.size() == 3);
    CHECK(hero[0].item_id == 812);
    CHECK(hero[0].count == 3200);
    CHECK(hero[1].item_id == 812);
    CHECK(hero[1].count == 23875);
    CHECK(hero[2].item_id == 953);
    CHECK(hero[2].count == 65);
    CHECK(runtime.money() == 27075);
}

TEST_CASE("inventory_runtime sell_trade creates a new hero money record") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({953, 51, 0x8000000Cu, 19, 0});
    auto runtime = InventoryRuntime::create(inventory, make_trade_names(), trade_template_for);
    runtime.set_person_items(std::string(kTradeNpc), {});
    runtime.set_person_trade_items(std::string(kTradeNpc), {InventoryItem{812, 42142, 0, 18, 0}});

    CHECK(runtime.sell_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 51)) ==
          TradeStatus::ok);

    const auto &hero = runtime.person_items("Hero");
    REQUIRE(hero.size() == 1);
    CHECK(hero[0].item_id == 812);
    CHECK(hero[0].count == 10608);
    CHECK(hero[0].type == 18);
    CHECK(hero[0].flags == 0xC0000000u);
    CHECK(hero[0].style == 1);
    CHECK(trade_item_count(runtime, 953) == 51);
    CHECK(runtime.trade_money(kTradeNpc) == 31534);
}

TEST_CASE("inventory_runtime sell_trade merge keeps the npc record flags") {
    gl::assets::HeroInventory inventory;
    inventory.items.push_back({812, 43875, 0, 18, 0});
    inventory.items.push_back({953, 51, 0x8000000Cu, 19, 0});
    auto runtime = InventoryRuntime::create(inventory, make_trade_names(), trade_template_for);
    runtime.set_person_items(std::string(kTradeNpc), {});
    runtime.set_person_trade_items(
        std::string(kTradeNpc),
        {InventoryItem{812, 42142, 0, 18, 0}, InventoryItem{953, 14, 0xABCDEF01u, 19, 7}});

    CHECK(runtime.sell_trade(kTradeNpc, "Hero", "POT_1_00_27", whole_stack_price(1, 51)) ==
          TradeStatus::ok);

    const auto &trade = runtime.trade_items(kTradeNpc);
    REQUIRE(trade.size() == 2);
    CHECK(trade[1].item_id == 953);
    CHECK(trade[1].count == 65);
    CHECK(trade[1].flags == 0xABCDEF01u);
    CHECK(trade[1].type == 19);
    CHECK(trade[1].style == 7);
}

TEST_CASE("inventory_runtime trade rejects a quote for a different item id") {
    auto runtime = make_trade_runtime(43875, 51, 42142, 14);
    const auto hero_before = runtime.person_items("Hero");
    const auto trade_before = runtime.trade_items(kTradeNpc);
    TradePrice price = whole_stack_price(1, 14);
    price.item_id = 1101;

    CHECK(runtime.buy_trade(kTradeNpc, "Hero", "POT_1_00_27", price) ==
          TradeStatus::unsupported_price);

    check_items_equal(runtime.person_items("Hero"), hero_before);
    check_items_equal(runtime.trade_items(kTradeNpc), trade_before);
}
