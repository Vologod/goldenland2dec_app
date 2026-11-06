#include "gl/game/inventory_runtime.hpp"

#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/vfs.hpp"

#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

gl::game::InventoryRuntime make_runtime(const gl::assets::GameVfs &vfs, int slot) {
    auto body = gl::assets::decode_save_body(read_save(slot));
    REQUIRE(body.has_value());
    auto inventory = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs);
    REQUIRE(names.has_value());
    return gl::game::InventoryRuntime::create(inventory.value(), names.value());
}

void check_slot(int slot, std::int32_t money, std::int32_t non_money_count) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto runtime = make_runtime(vfs.value(), slot);

    CHECK(runtime.count("Hero", "MON_1_0_1") == money);
    CHECK(runtime.count("Hero", "QST_1_0_159") == 0);
    CHECK(runtime.count("Hero", "ARM_1_08_2") == 0);
    CHECK(runtime.count("Hero", "RGT_1_0_8") == non_money_count);
    CHECK(runtime.money() == money);
}
} // namespace

TEST_CASE("real data: inventory_runtime slot0 oracle" * doctest::skip(!game_dir_available())) {
    check_slot(0, 43875, 6);
}

TEST_CASE("real data: inventory_runtime slot2 oracle" * doctest::skip(!game_dir_available())) {
    check_slot(2, 122008, 1);
}
