#include <doctest/doctest.h>

#include "gl/assets/bytes.hpp"
#include "gl/assets/cs.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/game_script_host.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/level.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/npc_loot.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
constexpr int kStepCap = 20000;
constexpr std::string_view kDialog =
    "scripts/dialogs/l1_1/l1_1.p387_poison_plotnik.d387.age.cs";
constexpr std::string_view kHero = "Hero";
constexpr std::string_view kP387 = "L1_1.P387_Poison_Plotnik";

bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

void set_int_global(std::vector<gl::script::ScriptGlobal> &globals, std::string_view name,
                    std::int32_t value) {
    for (gl::script::ScriptGlobal &global : globals) {
        if (global.name == name) {
            global.value = value;
            return;
        }
    }
    globals.push_back({std::string("int"), std::string(name), value});
}

std::int32_t numeric_value(const gl::script::Value &value) {
    if (const auto *item = std::get_if<std::int32_t>(&value)) return *item;
    if (const auto *item = std::get_if<std::uint32_t>(&value))
        return static_cast<std::int32_t>(*item);
    if (const auto *item = std::get_if<double>(&value))
        return static_cast<std::int32_t>(*item);
    return 0;
}

void run_until(gl::script::ScriptVm &vm, gl::script::VmStatus expected) {
    for (int step = 0; step < kStepCap && vm.status() != expected; ++step) {
        if (!vm.step()) break;
    }
    REQUIRE_FALSE(vm.failed());
    REQUIRE(vm.status() == expected);
}

std::vector<gl::script::ScriptGlobal> load_globals(gl::assets::GameVfs &vfs) {
    auto data = vfs.read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(data.has_value());
    auto globals = gl::script::parse_globals(data.value());
    REQUIRE(globals.has_value());
    return std::move(globals.value());
}

gl::game::InventoryRuntime::ItemTemplateLookup
catalog_lookup(std::shared_ptr<const gl::assets::ItemCatalog> catalog) {
    return [catalog](std::uint32_t item_id) -> std::optional<gl::game::ItemTemplate> {
        const gl::assets::ItemCatalogEntry *entry = catalog->find(item_id);
        if (entry == nullptr) return std::nullopt;
        return gl::game::ItemTemplate{static_cast<std::uint32_t>(entry->item.type),
                                      entry->item.flags, entry->item.unknown_b[1],
                                      entry->item.max_stack,
                                      static_cast<std::uint32_t>(entry->item.effects.size())};
    };
}

struct DialogFixture {
    std::shared_ptr<gl::assets::GameVfs> vfs;
    std::shared_ptr<const gl::assets::ItemCatalog> catalog;
    gl::assets::ItemNameIndex names;
    gl::game::Level level;
    gl::game::InventoryRuntime runtime;
};

DialogFixture make_fixture(int slot) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);

    auto mounted = gl::assets::GameVfs::mount(dir);
    REQUIRE(mounted.has_value());
    auto vfs = std::make_shared<gl::assets::GameVfs>(std::move(mounted.value()));
    auto loaded_catalog = gl::assets::ItemCatalog::load(*vfs);
    REQUIRE(loaded_catalog.has_value());
    auto catalog =
        std::make_shared<const gl::assets::ItemCatalog>(std::move(loaded_catalog.value()));
    auto names = gl::assets::ItemNameIndex::load(*vfs);
    REQUIRE(names.has_value());
    auto level = gl::game::load_level(*vfs, "l1_1");
    REQUIRE(level.has_value());
    auto body = gl::assets::decode_save_body(read_save(slot));
    REQUIRE(body.has_value());
    auto inventory = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory.has_value());

    gl::game::InventoryRuntime runtime = gl::game::InventoryRuntime::create(
        inventory.value(), names.value(), catalog_lookup(catalog));
    gl::game::MsvcRandom rng(1);
    gl::game::generate_npc_inventories(runtime, level->persons, *vfs, names.value(),
                                       catalog_lookup(catalog), 1, rng);

    return DialogFixture{vfs, catalog, std::move(names.value()), std::move(level.value()),
                         std::move(runtime)};
}

void check_transfer(std::string_view slot_name, int slot, bool preload) {
    INFO("slot " << slot_name);
    DialogFixture fixture = make_fixture(slot);

    CHECK(fixture.runtime.count(kHero, "QST_1_0_3") == 0);
    CHECK(fixture.runtime.count(kP387, "QST_1_0_3") == 0);
    CHECK(fixture.runtime.count(kP387, "QST_1_0_5") == 1);
    if (preload) REQUIRE(fixture.runtime.add_item(kHero, "QST_1_0_3", 1));

    auto script_data = fixture.vfs->read(kDialog);
    REQUIRE(script_data.has_value());
    auto nodes = gl::assets::parse_cs(script_data.value());
    REQUIRE(nodes.has_value());

    std::vector<gl::script::ScriptGlobal> globals = load_globals(*fixture.vfs);
    set_int_global(globals, "L1_Svetlograd_n3_GetElexir", 0);
    set_int_global(globals, "plotnik_povtor", 0);

    gl::script::NativeServices services;
    services.inventory = fixture.runtime.access();
    gl::game::GameScriptHost host(services);
    gl::script::ScriptVm vm(std::move(nodes.value()), host);
    vm.set_globals(globals);

    run_until(vm, gl::script::VmStatus::wait_user);
    const std::vector<std::uint32_t> first = vm.dialogs_data();
    if (preload) {
        REQUIRE(first == std::vector<std::uint32_t>{1006, 2190, 2191});
    } else {
        REQUIRE(first == std::vector<std::uint32_t>{1006, 2191});
    }

    REQUIRE(vm.user_input(0));
    if (preload) {
        run_until(vm, gl::script::VmStatus::wait_user);
        CHECK(vm.dialogs_data() == std::vector<std::uint32_t>{1203, 2194});
        CHECK(fixture.runtime.count(kHero, "QST_1_0_3") == 0);
        CHECK(fixture.runtime.count(kP387, "QST_1_0_3") == 1);
        CHECK(fixture.runtime.count(kP387, "QST_1_0_5") == 1);
        REQUIRE(vm.user_input(0));
        run_until(vm, gl::script::VmStatus::end);
        const gl::script::Value *elexir = vm.variable("L1_Svetlograd_n3_GetElexir");
        const gl::script::Value *repeat = vm.variable("plotnik_povtor");
        REQUIRE(elexir != nullptr);
        REQUIRE(repeat != nullptr);
        CHECK(numeric_value(*elexir) == 5);
        CHECK(numeric_value(*repeat) == 2);
    } else {
        run_until(vm, gl::script::VmStatus::end);
        CHECK(vm.dialogs_data().empty());
        CHECK(fixture.runtime.count(kHero, "QST_1_0_3") == 0);
        CHECK(fixture.runtime.count(kP387, "QST_1_0_3") == 0);
        CHECK(fixture.runtime.count(kP387, "QST_1_0_5") == 1);
        const gl::script::Value *repeat = vm.variable("plotnik_povtor");
        REQUIRE(repeat != nullptr);
        CHECK(numeric_value(*repeat) == 1);
    }

    CHECK(fixture.runtime.count(kHero, "QST_1_0_3") == 0);
    CHECK(fixture.runtime.count(kP387, "QST_1_0_3") == (preload ? 1 : 0));
}
} // namespace

TEST_CASE("real dialog: slot0 transfers QST_1_0_3 to the poison plotnik" *
          doctest::skip(!game_dir_available())) {
    check_transfer("slot0", 0, true);
}

TEST_CASE("real dialog: slot2 transfers QST_1_0_3 to the poison plotnik" *
          doctest::skip(!game_dir_available())) {
    check_transfer("slot2", 2, true);
}

TEST_CASE("real dialog: slot0 without QST_1_0_3 skips the transfer" *
          doctest::skip(!game_dir_available())) {
    check_transfer("slot0", 0, false);
}
