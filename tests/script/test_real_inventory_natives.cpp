#include <doctest/doctest.h>
#include "gl/assets/cs.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/game_script_host.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

std::int32_t numeric_value(const gl::script::Value &value) {
    if (const auto *item = std::get_if<std::int32_t>(&value)) return *item;
    if (const auto *item = std::get_if<std::uint32_t>(&value))
        return static_cast<std::int32_t>(*item);
    if (const auto *item = std::get_if<double>(&value))
        return static_cast<std::int32_t>(*item);
    return 0;
}

void check_slot(int slot, std::int32_t expected_money) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto script = vfs->read("scripts/dialogs/l13_1/l13_1.p517_boat_trader.d517.age.cs");
    auto globals_data = vfs->read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(script.has_value());
    REQUIRE(globals_data.has_value());
    auto nodes = gl::assets::parse_cs(script.value());
    auto globals = gl::script::parse_globals(globals_data.value());
    REQUIRE(nodes.has_value());
    REQUIRE(globals.has_value());

    auto body = gl::assets::decode_save_body(read_save(slot));
    REQUIRE(body.has_value());
    auto inventory = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    gl::game::InventoryRuntime runtime =
        gl::game::InventoryRuntime::create(inventory.value(), names.value());

    gl::script::NativeServices services;
    services.inventory = runtime.access();
    gl::game::GameScriptHost host(services);
    gl::script::ScriptVm vm(std::move(nodes.value()), host);
    vm.set_globals(globals.value());

    for (int i = 0; i < 2000 && vm.status() != gl::script::VmStatus::wait_user; ++i) {
        if (!vm.step()) break;
    }
    REQUIRE(vm.status() == gl::script::VmStatus::wait_user);

    const gl::script::Value *money = vm.variable("money");
    REQUIRE(money != nullptr);
    CHECK(numeric_value(*money) == expected_money);
}
} // namespace

TEST_CASE("real dialog: slot0 inventory count reaches money global" *
          doctest::skip(!game_dir_available())) {
    check_slot(0, 43875);
}

TEST_CASE("real dialog: slot2 inventory count reaches money global" *
          doctest::skip(!game_dir_available())) {
    check_slot(2, 122008);
}
