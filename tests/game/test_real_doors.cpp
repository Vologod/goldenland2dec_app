#include "gl/assets/vfs.hpp"
#include "gl/game/door_runtime.hpp"
#include "gl/game/level.hpp"

#include <algorithm>
#include <cstdlib>
#include <doctest/doctest.h>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

const gl::game::LevelDoor *find_door(const gl::game::Level &level, std::string_view name) {
    const auto it = std::find_if(level.doors.begin(), level.doors.end(),
                                 [name](const gl::game::LevelDoor &door) {
                                     return door.name == name;
                                 });
    return it == level.doors.end() ? nullptr : &*it;
}
} // namespace

TEST_CASE("real l10_1 closes door cells at load" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l10_1");
    REQUIRE(level.has_value());

    const gl::game::LevelDoor *door = find_door(level.value(), "door_storozhka");
    REQUIRE(door != nullptr);
    CHECK(door->state == 0);
    REQUIRE_FALSE(door->cells.empty());
    for (const auto &cell : door->cells)
        CHECK_FALSE(level->walkable.is_walkable(cell.first, cell.second));

    gl::game::DoorRuntime runtime;
    runtime.reset(level->doors);
    CHECK(runtime.set("door_storozhka", 1, level->walkable));
    for (const auto &cell : door->cells)
        CHECK(level->walkable.is_walkable(cell.first, cell.second));
    CHECK(runtime.set("door_storozhka", 0, level->walkable));
    for (const auto &cell : door->cells)
        CHECK_FALSE(level->walkable.is_walkable(cell.first, cell.second));
    CHECK_FALSE(runtime.set("door_missing", 1, level->walkable));
}

TEST_CASE("real l10_1_2 starts with an open door" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l10_1_2");
    REQUIRE(level.has_value());

    const gl::game::LevelDoor *door = find_door(level.value(), "door_1");
    REQUIRE(door != nullptr);
    CHECK(door->state == 1);
    REQUIRE_FALSE(door->cells.empty());
    for (const auto &cell : door->cells)
        CHECK(level->walkable.is_walkable(cell.first, cell.second));
}
