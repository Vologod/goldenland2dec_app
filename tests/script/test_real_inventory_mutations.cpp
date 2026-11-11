#include <doctest/doctest.h>

#include "gl/assets/bytes.hpp"
#include "gl/assets/cs.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/game_script_host.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/script/dialog_session.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

void check_slot(int slot, std::int32_t start_money, std::int32_t expected_money) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);

    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto script = vfs->read("scripts/dialogs/unique/gypsy.age.cs");
    REQUIRE(script.has_value());
    auto globals_data = vfs->read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(globals_data.has_value());
    auto sdb_data = vfs->read("sdb/dialogs/dialogsphrases.sdb");
    REQUIRE(sdb_data.has_value());

    auto nodes = gl::assets::parse_cs(script.value());
    REQUIRE(nodes.has_value());
    auto globals = gl::script::parse_globals(globals_data.value());
    REQUIRE(globals.has_value());
    auto sdb = gl::assets::Sdb::parse(sdb_data.value());
    REQUIRE(sdb.has_value());

    auto body = gl::assets::decode_save_body(read_save(slot));
    REQUIRE(body.has_value());
    auto inventory = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory.has_value());
    auto names = gl::assets::ItemNameIndex::load(vfs.value());
    REQUIRE(names.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    auto item_template = [&catalog](std::uint32_t item_id)
        -> std::optional<gl::game::ItemTemplate> {
        const gl::assets::ItemCatalogEntry *entry = catalog->find(item_id);
        if (entry == nullptr) return std::nullopt;
        std::vector<gl::game::ItemTemplateEffect> effects;
        effects.reserve(entry->item.effects.size());
        for (const gl::assets::ItemEffect &effect : entry->item.effects)
            effects.push_back({effect.type, static_cast<std::int32_t>(effect.amount), effect.flags});
        return gl::game::ItemTemplate{static_cast<std::uint32_t>(entry->item.type),
                                      entry->item.flags, entry->item.unknown_b[1],
                                      entry->item.max_stack,
                                      static_cast<std::uint32_t>(effects.size()),
                                      std::move(effects)};
    };
    gl::game::InventoryRuntime runtime =
        gl::game::InventoryRuntime::create(inventory.value(), names.value(), item_template);

    gl::script::NativeServices services;
    services.inventory = runtime.access();
    gl::game::GameScriptHost host(services);
    gl::script::ScriptVm vm(std::move(nodes.value()), host);
    vm.set_globals(globals.value());

    gl::script::DialogSession session(vm, sdb.value());
    REQUIRE(session.start().has_value());
    REQUIRE(session.status() == gl::script::VmStatus::wait_user);
    REQUIRE(session.waiting());
    CHECK(runtime.money() == start_money);
    CHECK(runtime.count("Hero", "MON_1_0_1") == start_money);

    REQUIRE(session.choose(1).has_value());
    REQUIRE_FALSE(session.failed());
    REQUIRE(session.waiting());

    REQUIRE(session.choose(0).has_value());
    CHECK_FALSE(vm.failed());
    CHECK_FALSE(session.failed());

    CHECK(runtime.money() == expected_money);
    CHECK(runtime.count("Hero", "MON_1_0_1") == expected_money);
}
} // namespace

TEST_CASE("real dialog: slot0 inventory mutation removes money" *
          doctest::skip(!game_dir_available())) {
    check_slot(0, 43875, 41075);
}

TEST_CASE("real dialog: slot2 inventory mutation removes money" *
          doctest::skip(!game_dir_available())) {
    check_slot(2, 122008, 119208);
}
