#include "gl/game/inventory_runtime.hpp"

#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/npc_loot.hpp"
#include "gl/game/person.hpp"

#include <doctest/doctest.h>

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
    const std::int32_t hero_rgt = runtime.count("Hero", "RGT_1_0_8");

    gl::game::MsvcRandom rng(1);
    gl::game::generate_npc_inventories(runtime, level->persons, vfs.value(), names.value(),
                                       catalog_lookup(catalog.value()), 1, rng);

    CHECK(runtime.count("L1_1.P387_Poison_Plotnik", "QST_1_0_5") == 1);
    CHECK(runtime.count("L0.P7_Kolbasens_Rodstvennik1", "MON_1_0_1") == 174);
    CHECK(runtime.count("L1_1.P389_Crying_Guardian", "MON_1_0_1") == 218);
    CHECK(runtime.count("L0.P711_citizen8", "MON_1_0_1") == 323);
    CHECK(runtime.person_items("L0.P678_citizen4").empty());

    CHECK(runtime.count("Hero", "RGT_1_0_8") == hero_rgt);
    CHECK(runtime.money() == hero_money);
    CHECK(runtime.count("Hero", "MON_1_0_1") == hero_money);
}
