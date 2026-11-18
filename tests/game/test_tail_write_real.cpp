#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/save_restore.hpp"
#include "gl/game/tail_write.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

TEST_CASE("real data: selected tail no-change build is byte-identical" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    for (int slot : {0, 9, 21}) {
        CAPTURE(slot);
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        REQUIRE(payload->level_tail_index.has_value());
        const gl::assets::SaveTailEntry &tail =
            payload->tail[*payload->level_tail_index];

        auto level = gl::game::load_level(vfs.value(), payload->info.level_id);
        REQUIRE(level.has_value());
        auto inventory = gl::game::InventoryRuntime::create({}, {}, {});
        const auto restore = gl::game::restore_save_persons(
            level.value(), tail, &payload->tribes, false, &inventory);

        auto simulation = gl::game::LevelSimulation::create(level.value(), 1);
        for (const auto &person : restore.persons)
            simulation.restore_person(person.level_index, person.current, person.direction,
                                      person.route_index);
        const std::vector<gl::game::TailPersonLive> live(restore.persons.size());

        const auto rebuilt = gl::game::build_selected_tail(
            tail, level.value(), simulation, inventory, restore, live, {}, tail.rginf,
            &payload->tribes);
        REQUIRE(rebuilt.has_value());
        CHECK(rebuilt->section == tail.section);
    }
}
