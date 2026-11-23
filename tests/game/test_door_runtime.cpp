#include "gl/game/door_runtime.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/script/native_registry.hpp"

#include <array>
#include <doctest/doctest.h>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

using gl::game::DoorRuntime;
using gl::game::LevelDoor;
using gl::game::WalkGrid;
using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::Value;

namespace {
Value call(std::uint32_t id, std::initializer_list<Value> args, NativeServices &services) {
    const std::vector<Value> values(args);
    const auto *info = NativeRegistry::find(id);
    REQUIRE(info != nullptr);
    return NativeRegistry::call(*info, values, services);
}
} // namespace

TEST_CASE("door runtime blocks closed and clears open") {
    LevelDoor door;
    door.name = "door_a";
    door.state = 0;
    door.cells = {{1, 1}, {2, 1}};
    const std::vector<LevelDoor> doors{door};

    WalkGrid grid;
    grid.width = 4;
    grid.height = 2;
    grid.walkable.assign(8, 1);

    DoorRuntime runtime;
    runtime.reset(doors);
    REQUIRE(runtime.size() == 1);
    CHECK(runtime.set("door_a", 0, grid));
    CHECK_FALSE(grid.is_walkable(1, 1));
    CHECK_FALSE(grid.is_walkable(2, 1));
    CHECK(runtime.set("door_a", 1, grid));
    CHECK(grid.is_walkable(1, 1));
    CHECK(grid.is_walkable(2, 1));
    CHECK(runtime.set("door_a", 1, grid));
    CHECK(grid.is_walkable(1, 1));
    CHECK_FALSE(runtime.set("Door_A", 0, grid));
    CHECK_FALSE(runtime.set("missing", 0, grid));
    CHECK(grid.is_walkable(1, 1));
    CHECK(grid.is_walkable(2, 1));
    REQUIRE(runtime.find("door_a") != nullptr);
    CHECK(runtime.find("door_a")->state == 1);
    CHECK(runtime.find("missing") == nullptr);
    runtime.clear();
    CHECK(runtime.size() == 0);
}

TEST_CASE("level door resolution requires an exact lvl record and cell group") {
    gl::assets::SefDoor sef;
    sef.name = "door_a";
    sef.cells_name = "ignored";
    sef.is_opened = 1;
    gl::assets::LvlDoor lvl;
    lvl.name = "door_a";
    lvl.cell_group = "group_a";
    gl::assets::LvlCellGroup group;
    group.name = "group_a";
    group.cells = {{1, 2}, {3, 4}};
    const std::array<gl::assets::LvlDoor, 1> lvl_doors{lvl};
    const std::array<gl::assets::LvlCellGroup, 1> groups{group};

    const auto resolved = gl::game::resolve_level_door(sef, lvl_doors, groups);
    REQUIRE(resolved.has_value());
    CHECK(resolved->name == "door_a");
    CHECK(resolved->state == 1);
    REQUIRE(resolved->cells.size() == 2);
    CHECK(resolved->cells[0] == std::pair<std::int32_t, std::int32_t>{1, 2});
    CHECK(resolved->cells[1] == std::pair<std::int32_t, std::int32_t>{3, 4});

    gl::assets::SefDoor missing_lvl;
    missing_lvl.name = "missing";
    CHECK_FALSE(gl::game::resolve_level_door(missing_lvl, lvl_doors, groups).has_value());

    gl::assets::LvlDoor bad_group;
    bad_group.name = "door_a";
    bad_group.cell_group = "missing_group";
    const std::array<gl::assets::LvlDoor, 1> bad_doors{bad_group};
    CHECK_FALSE(gl::game::resolve_level_door(sef, bad_doors, groups).has_value());
}

TEST_CASE("set-door-state native dispatches and is supported") {
    LevelDoor door;
    door.name = "door_a";
    door.cells = {{1, 1}};
    WalkGrid grid;
    grid.width = 3;
    grid.height = 3;
    grid.walkable.assign(9, 1);
    DoorRuntime runtime;
    runtime.reset(std::span<const LevelDoor>(&door, 1));

    NativeServices services;
    int calls = 0;
    services.door.set = [&](std::string_view name, std::int32_t state) {
        ++calls;
        return runtime.set(name, state, grid);
    };
    CHECK(call(0x7000014, {std::string{"door_a"}, std::int32_t{0}}, services) ==
          Value{double{0.0}});
    CHECK(calls == 1);
    CHECK_FALSE(grid.is_walkable(1, 1));
    CHECK(call(0x7000014, {std::string{"door_a"}, double{1.0}}, services) ==
          Value{double{0.0}});
    CHECK(calls == 2);
    CHECK(grid.is_walkable(1, 1));
    CHECK(call(0x7000014, {std::string{"door_a"}, std::string{"x"}}, services) ==
          Value{double{0.0}});
    CHECK(calls == 2);
    CHECK(call(0x7000014, {std::int32_t{1}, std::int32_t{0}}, services) ==
          Value{double{0.0}});
    CHECK(calls == 2);

    const auto *info = NativeRegistry::find(0x7000014);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 2);
    CHECK(info->handler != nullptr);
}

TEST_CASE("door runtime refresh updates simulation grid") {
    gl::game::Level level;
    level.walkable.width = 2;
    level.walkable.height = 1;
    level.walkable.walkable = {1, 1};
    auto sim = gl::game::LevelSimulation::create(level, 1);
    level.walkable.set_walkable(0, 0, false);
    sim.refresh_walk_grid(level.walkable);
    CHECK_FALSE(sim.grid().is_walkable(0, 0));
    CHECK(sim.grid().is_walkable(1, 0));
}
