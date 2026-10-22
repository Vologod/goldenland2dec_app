#include "gl/assets/lvl.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/walk_grid.hpp"
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>

using namespace gl::game;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("walk grid maps chunk tiles to world tiles") {
    gl::assets::LvlData lvl;
    lvl.chunk_width = 2;
    lvl.chunk_height = 1;
    lvl.chunks.resize(2);
    // chunk 0 tiles: (0,0) free, (1,0) blocked, (0,1) free, (1,1) blocked
    lvl.chunks[0][0].mask = 0xFFFF;
    lvl.chunks[0][1].mask = 0x0000;
    lvl.chunks[0][2].mask = 0xFFFF;
    lvl.chunks[0][3].mask = 0x0000;
    lvl.chunks[1][0].mask = 0xFFFF;
    lvl.chunks[1][1].mask = 0xFFFF;
    lvl.chunks[1][2].mask = 0xFFFF;
    lvl.chunks[1][3].mask = 0xFFFF;

    const WalkGrid grid = build_walk_grid(lvl);
    CHECK(grid.width == 4);
    CHECK(grid.height == 2);
    CHECK(grid.is_walkable(0, 0));
    CHECK_FALSE(grid.is_walkable(1, 0));
    CHECK(grid.is_walkable(0, 1));
    CHECK_FALSE(grid.is_walkable(1, 1));
    CHECK(grid.is_walkable(3, 1));
    CHECK_FALSE(grid.is_walkable(-1, 0));
    CHECK_FALSE(grid.is_walkable(0, 2));
}

TEST_CASE("walk grid handles second chunk row and intermediate mask") {
    gl::assets::LvlData lvl;
    lvl.chunk_width = 1;
    lvl.chunk_height = 2;
    lvl.chunks.resize(2);
    lvl.chunks[0][0].mask = 0xFFFF;
    lvl.chunks[0][1].mask = 0xFFFF;
    lvl.chunks[0][2].mask = 0xFFFF;
    lvl.chunks[0][3].mask = 0xFFFF;
    lvl.chunks[1][0].mask = 0x0100;
    lvl.chunks[1][1].mask = 0x01FF;
    lvl.chunks[1][2].mask = 0x00FF;
    lvl.chunks[1][3].mask = 0x0000;

    const WalkGrid grid = build_walk_grid(lvl);
    CHECK(grid.width == 2);
    CHECK(grid.height == 4);
    CHECK(grid.is_walkable(0, 2));
    CHECK(grid.is_walkable(1, 2));
    CHECK_FALSE(grid.is_walkable(0, 3));
    CHECK_FALSE(grid.is_walkable(1, 3));
}

TEST_CASE("empty lvl produces empty grid") {
    gl::assets::LvlData lvl;
    CHECK(build_walk_grid(lvl).empty());
}

TEST_CASE("real data: l1_1 walk grid" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    CHECK(level->walkable.width == 230);
    CHECK(level->walkable.height == 224);
    std::size_t blocked = 0;
    for (std::uint8_t value : level->walkable.walkable)
        if (value == 0) ++blocked;
    CHECK(blocked == 10521);
}
