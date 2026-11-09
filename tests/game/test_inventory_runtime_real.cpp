#include "gl/game/inventory_runtime.hpp"

#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/npc_loot.hpp"
#include "gl/game/person.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

gl::game::InventoryRuntime::ItemTemplateLookup
catalog_lookup(const gl::assets::ItemCatalog &catalog) {
    return [&catalog](std::uint32_t item_id) -> std::optional<gl::game::ItemTemplate> {
        const gl::assets::ItemCatalogEntry *entry = catalog.find(item_id);
        if (entry == nullptr)
            return std::nullopt;
        return gl::game::ItemTemplate{static_cast<std::uint32_t>(entry->item.type),
                                      entry->item.flags, entry->item.unknown_b[1],
                                      entry->item.max_stack};
    };
}

gl::game::InventoryRuntime make_runtime(const gl::assets::GameVfs &vfs, int slot,
                                        const gl::assets::ItemCatalog &catalog) {
    auto body = gl::assets::decode_save_body(read_save(slot));
    REQUIRE(body.has_value());
    auto inventory = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs);
    REQUIRE(names.has_value());
    return gl::game::InventoryRuntime::create(inventory.value(), names.value(),
                                              catalog_lookup(catalog));
}

// Every oracle row builds a fresh runtime at base state; rows are not cumulative.
gl::game::InventoryRuntime make_transfer_scenario(gl::assets::GameVfs &vfs, int slot,
                                                  const gl::assets::ItemCatalog &catalog,
                                                  const gl::assets::ItemNameIndex &names,
                                                  const gl::game::Level &level) {
    auto runtime = make_runtime(vfs, slot, catalog);
    gl::game::MsvcRandom rng(1);
    gl::game::generate_npc_inventories(runtime, level.persons, vfs, names, catalog_lookup(catalog),
                                       1, rng);
    return runtime;
}

void check_transfer_item_slot(gl::assets::GameVfs &vfs, const gl::assets::ItemCatalog &catalog,
                              const gl::assets::ItemNameIndex &names, const gl::game::Level &level,
                              int slot, std::int32_t money, std::int32_t potion) {
    const std::string p7 = "L0.P7_Kolbasens_Rodstvennik1";
    const std::string p387 = "L1_1.P387_Poison_Plotnik";
    const std::string p678 = "L0.P678_citizen4";

    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_item("Hero", p7, "MON_1_0_1", 100));
        CHECK(runtime.count("Hero", "MON_1_0_1") == money - 100);
        CHECK(runtime.count(p7, "MON_1_0_1") == 274);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_item(p7, "Hero", "MON_1_0_1", 100));
        CHECK(runtime.count(p7, "MON_1_0_1") == 74);
        CHECK(runtime.count("Hero", "MON_1_0_1") == money + 100);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_item(p7, "Hero", "MON_1_0_1", 1000));
        CHECK(runtime.count(p7, "MON_1_0_1") == 0);
        CHECK(runtime.count("Hero", "MON_1_0_1") == money + 174);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_item("Hero", p7, "MON_1_0_1", -1));
        CHECK(runtime.count("Hero", "MON_1_0_1") == 0);
        CHECK(runtime.count(p7, "MON_1_0_1") == 174 + money);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_item("Hero", p7, "MON_1_0_1", 0));
        CHECK(runtime.count("Hero", "MON_1_0_1") == money);
        CHECK(runtime.count(p7, "MON_1_0_1") == 174);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        CHECK_FALSE(runtime.transfer_item("Hero", p387, "QST_1_0_5", 1));
        CHECK(runtime.count("Hero", "QST_1_0_5") == 0);
        CHECK(runtime.count(p387, "QST_1_0_5") == 1);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_item("Hero", p678, "POT_1_00_27", 10));
        CHECK(runtime.count("Hero", "POT_1_00_27") == potion - 10);
        CHECK(runtime.count(p678, "POT_1_00_27") == 10);
        REQUIRE(runtime.person_items(p678).size() == 1);
        const auto &item = runtime.person_items(p678)[0];
        CHECK(item.type == 19);
        CHECK(item.flags == 0x8000000Cu);
        CHECK(item.style == 1);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_item(p387, "Hero", "QST_1_0_5", 1));
        CHECK(runtime.count(p387, "QST_1_0_5") == 0);
        CHECK(runtime.count("Hero", "QST_1_0_5") == 1);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.add_item("Hero", "QST_1_0_3", 1));
        REQUIRE(runtime.transfer_item("Hero", p387, "QST_1_0_3", 1));
        CHECK(runtime.count("Hero", "QST_1_0_3") == 0);
        CHECK(runtime.count(p387, "QST_1_0_3") == 1);
        CHECK(runtime.count(p387, "QST_1_0_5") == 1);
        const auto &items = runtime.person_items(p387);
        const auto it =
            std::find_if(items.begin(), items.end(),
                         [](const gl::assets::InventoryItem &item) { return item.item_id == 973; });
        REQUIRE(it != items.end());
        CHECK(it->type == 25);
        CHECK(it->flags == 0xC0000028u);
        CHECK(it->style == 15);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_item("Hero", "Hero", "MON_1_0_1", 100));
        CHECK(runtime.count("Hero", "MON_1_0_1") == money);
        CHECK(runtime.count(p7, "MON_1_0_1") == 174);
    }
}

void check_transfer_all_slot(gl::assets::GameVfs &vfs, const gl::assets::ItemCatalog &catalog,
                             const gl::assets::ItemNameIndex &names, const gl::game::Level &level,
                             int slot, std::int32_t money, std::int32_t potion,
                             std::int32_t regent) {
    const std::string p387 = "L1_1.P387_Poison_Plotnik";
    const std::string p678 = "L0.P678_citizen4";

    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_all(p387, "Hero"));
        CHECK(runtime.person_items(p387).empty());
        CHECK(runtime.count("Hero", "QST_1_0_5") == 1);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_all(p678, "Hero"));
        CHECK(runtime.count("Hero", "MON_1_0_1") == money);
        CHECK(runtime.count("Hero", "POT_1_00_27") == potion);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_all("Hero", p678));
        CHECK(runtime.hero_inventory().items.empty());
        CHECK(runtime.money() == 0);
        CHECK(runtime.count(p678, "MON_1_0_1") == money);
        CHECK(runtime.count(p678, "POT_1_00_27") == potion);
        CHECK(runtime.count(p678, "RGT_1_0_8") == regent);
    }
    {
        auto runtime = make_transfer_scenario(vfs, slot, catalog, names, level);
        REQUIRE(runtime.transfer_all("Hero", "Hero"));
        CHECK(runtime.count("Hero", "MON_1_0_1") == money);
        CHECK(runtime.count("Hero", "POT_1_00_27") == potion);
    }
}

void check_slot(int slot, std::int32_t money, std::int32_t non_money_count,
                std::int32_t potion_count) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto runtime = make_runtime(vfs.value(), slot, catalog.value());

    CHECK(runtime.count("Hero", "MON_1_0_1") == money);
    CHECK(runtime.count("Hero", "QST_1_0_159") == 0);
    CHECK(runtime.count("Hero", "ARM_1_08_2") == 0);
    CHECK(runtime.count("Hero", "RGT_1_0_8") == non_money_count);
    CHECK(runtime.count("Hero", "POT_1_00_27") == potion_count);
    CHECK(runtime.money() == money);

    REQUIRE(runtime.add_item("Hero", "MON_1_0_1", 100));
    CHECK(runtime.count("Hero", "MON_1_0_1") == money + 100);
    CHECK(runtime.money() == money + 100);
    REQUIRE(runtime.remove_item("Hero", "MON_1_0_1", 100));
    CHECK(runtime.count("Hero", "MON_1_0_1") == money);
    CHECK(runtime.money() == money);

    REQUIRE(runtime.add_item("Hero", "POT_1_00_27", 10));
    CHECK(runtime.count("Hero", "POT_1_00_27") == potion_count + 10);
    REQUIRE(runtime.remove_item("Hero", "POT_1_00_27", 10));
    CHECK(runtime.count("Hero", "POT_1_00_27") == potion_count);

    REQUIRE(runtime.add_item("Hero", "ARM_1_08_2", 1));
    CHECK(runtime.count("Hero", "ARM_1_08_2") == 1);
    REQUIRE(runtime.remove_item("Hero", "ARM_1_08_2", 1));
    CHECK(runtime.count("Hero", "ARM_1_08_2") == 0);
}
} // namespace

TEST_CASE("real data: inventory_runtime slot0 oracle" * doctest::skip(!game_dir_available())) {
    check_slot(0, 43875, 6, 51);
}

TEST_CASE("real data: inventory_runtime slot2 oracle" * doctest::skip(!game_dir_available())) {
    check_slot(2, 122008, 1, 25);
}

TEST_CASE("real data: inventory_runtime transfer_item slot0 oracle" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    check_transfer_item_slot(vfs.value(), catalog.value(), names.value(), level.value(), 0, 43875,
                             51);
}

TEST_CASE("real data: inventory_runtime transfer_item slot2 oracle" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    check_transfer_item_slot(vfs.value(), catalog.value(), names.value(), level.value(), 2, 122008,
                             25);
}

TEST_CASE("real data: inventory_runtime transfer_all slot0 oracle" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    check_transfer_all_slot(vfs.value(), catalog.value(), names.value(), level.value(), 0, 43875,
                            51, 6);
}

TEST_CASE("real data: inventory_runtime transfer_all slot2 oracle" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    check_transfer_all_slot(vfs.value(), catalog.value(), names.value(), level.value(), 2, 122008,
                            25, 1);
}

TEST_CASE("real data: l1_1 npc inventories match the pass-2 reference" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    REQUIRE(level->persons.size() == 16);

    auto runtime = make_runtime(vfs.value(), 21, catalog.value());
    const std::int32_t hero_money = runtime.money();
    const std::int32_t hero_mon = runtime.count("Hero", "MON_1_0_1");
    const std::int32_t hero_rgt = runtime.count("Hero", "RGT_1_0_8");
    const std::int32_t hero_potion = runtime.count("Hero", "POT_1_00_27");
    const std::size_t hero_records = runtime.hero_inventory().items.size();

    gl::game::MsvcRandom rng(1);
    gl::game::generate_npc_inventories(runtime, level->persons, vfs.value(), names.value(),
                                       catalog_lookup(catalog.value()), 1, rng);
    const gl::game::MsvcRandom rng_after_loot = rng;

    CHECK(runtime.count("L1_1.P387_Poison_Plotnik", "QST_1_0_5") == 1);
    CHECK(runtime.count("L0.P7_Kolbasens_Rodstvennik1", "MON_1_0_1") == 174);
    CHECK(runtime.count("L1_1.P389_Crying_Guardian", "MON_1_0_1") == 218);
    CHECK(runtime.count("L0.P711_citizen8", "MON_1_0_1") == 323);
    CHECK(runtime.person_items("L0.P678_citizen4").empty());
    CHECK(runtime.access().item_count("L1_1.P387_Poison_Plotnik", "QST_1_0_5") == 1);
    REQUIRE_FALSE(runtime.trade_items("L1_1.P387_Poison_Plotnik").empty());
    CHECK(runtime.trade_money("L1_1.P387_Poison_Plotnik") == 0);
    REQUIRE(runtime.trade_items("L1_1.P387_Poison_Plotnik").size() == 1);
    CHECK(runtime.trade_items("L1_1.P387_Poison_Plotnik")[0].item_id == 975);
    CHECK(runtime.trade_items("L1_1.P387_Poison_Plotnik")[0].type == 25);
    CHECK(runtime.trade_items("L1_1.P387_Poison_Plotnik")[0].count == 1);

    REQUIRE(runtime.person_items("L0.P744_oficer1").size() == 2);
    CHECK(runtime.person_items("L0.P744_oficer1")[0].item_id == 263);
    CHECK(runtime.person_items("L0.P744_oficer1")[1].item_id == 371);
    CHECK(runtime.count("L0.P744_oficer1", "MON_1_0_1") == 0);

    gl::game::LevelSimulation loot_sim = gl::game::LevelSimulation::create(level.value(), rng);
    gl::game::LevelSimulation loot_sim_copy =
        gl::game::LevelSimulation::create(level.value(), rng_after_loot);
    gl::game::LevelSimulation fresh_sim =
        gl::game::LevelSimulation::create(level.value(), gl::game::MsvcRandom(1));
    const std::uint32_t loot_next = loot_sim.rng().next();
    CHECK(loot_next == loot_sim_copy.rng().next());
    CHECK(loot_next != fresh_sim.rng().next());

    CHECK(runtime.count("Hero", "MON_1_0_1") == hero_mon);
    CHECK(runtime.count("Hero", "RGT_1_0_8") == hero_rgt);
    CHECK(runtime.count("Hero", "POT_1_00_27") == hero_potion);
    CHECK(runtime.hero_inventory().items.size() == hero_records);
    CHECK(runtime.money() == hero_money);
}
