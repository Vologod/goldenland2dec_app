#include "gl/game/pathfinder.hpp"
#include <doctest/doctest.h>

using namespace gl::game;

namespace {
WalkGrid grid_with_wall() {
    WalkGrid grid;
    grid.width = 7;
    grid.height = 5;
    grid.walkable.assign(static_cast<std::size_t>(grid.width) * grid.height, 1);
    for (std::int32_t y = 0; y < 4; ++y)
        grid.walkable[static_cast<std::size_t>(y) * grid.width + 3] = 0;
    return grid;
}
const std::vector<std::uint8_t> kFree;
}

TEST_CASE("line of sight is blocked by walls") {
    const WalkGrid grid = grid_with_wall();
    CHECK(line_of_sight(grid, {0, 4}, {6, 4}, kFree));
    CHECK_FALSE(line_of_sight(grid, {0, 0}, {6, 0}, kFree));
    CHECK(line_of_sight(grid, {2, 2}, {2, 2}, kFree));
}

TEST_CASE("find_path goes around a wall") {
    const WalkGrid grid = grid_with_wall();
    const auto path = find_path(grid, {0, 0}, {6, 0}, kFree);
    REQUIRE_FALSE(path.empty());
    CHECK(path.front() == Tile{0, 0});
    CHECK(path.back() == Tile{6, 0});
    for (const Tile& tile : path)
        CHECK(grid.is_walkable(tile.x, tile.y));
    CHECK(path.size() > 7);
}

TEST_CASE("find_path returns empty for unreachable target") {
    WalkGrid grid = grid_with_wall();
    for (std::int32_t x = 0; x < grid.width; ++x)
        grid.walkable[static_cast<std::size_t>(4) * grid.width + x] = 0;
    CHECK(find_path(grid, {0, 0}, {0, 4}, kFree).empty());
}

TEST_CASE("blocked cells are obstacles but the start cell is free") {
    const WalkGrid grid = grid_with_wall();
    std::vector<std::uint8_t> blocked(static_cast<std::size_t>(grid.width) * grid.height, 0);
    blocked[static_cast<std::size_t>(2) * grid.width + 1] = 1;
    blocked[static_cast<std::size_t>(2) * grid.width + 2] = 1;
    CHECK_FALSE(line_of_sight(grid, {0, 2}, {4, 2}, blocked));
    const auto path = find_path(grid, {0, 2}, {4, 2}, blocked);
    REQUIRE_FALSE(path.empty());
    for (const Tile& tile : path) {
        const bool free = blocked[static_cast<std::size_t>(tile.y) * grid.width + tile.x] == 0 ||
                          tile == Tile{0, 2};
        CHECK(free);
    }
}
