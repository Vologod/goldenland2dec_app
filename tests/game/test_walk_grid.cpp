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
    // Chunk 0: first tile relief bits 9 and 11 -> sub-tiles 1 and 3 blocked.
    lvl.chunks[0][0].relief = 0x0A00;
    // The remaining tiles of the chunk are ignored.
    lvl.chunks[0][1].relief = 0xFFFF;
    lvl.chunks[0][2].relief = 0xFFFF;
    lvl.chunks[0][3].relief = 0xFFFF;
    // Chunk 1: first tile relief bits 8..11 -> all four sub-tiles blocked.
    lvl.chunks[1][0].relief = 0x0F00;

    const WalkGrid grid = build_walk_grid(lvl);
    CHECK(grid.width == 4);
    CHECK(grid.height == 2);
    CHECK(grid.is_walkable(0, 0));
    CHECK_FALSE(grid.is_walkable(1, 0));
    CHECK(grid.is_walkable(0, 1));
    CHECK_FALSE(grid.is_walkable(1, 1));
    CHECK_FALSE(grid.is_walkable(2, 0));
    CHECK_FALSE(grid.is_walkable(3, 0));
    CHECK_FALSE(grid.is_walkable(2, 1));
    CHECK_FALSE(grid.is_walkable(3, 1));
    CHECK_FALSE(grid.is_walkable(-1, 0));
    CHECK_FALSE(grid.is_walkable(0, 2));
}

TEST_CASE("walk grid maps relief bits 8..11 to chunk sub-tiles") {
    gl::assets::LvlData lvl;
    lvl.chunk_width = 1;
    lvl.chunk_height = 1;
    lvl.chunks.resize(1);
    // Relief bits 8 and 10 -> sub = (x & 1) + 2 * (y & 1) equals 0 and 2.
    lvl.chunks[0][0].relief = 0x0500;
    lvl.chunks[0][0].sound = 0xFFFF;
    lvl.chunks[0][1].relief = 0x0F00;
    lvl.chunks[0][2].relief = 0x0F00;
    lvl.chunks[0][3].relief = 0x0F00;

    const WalkGrid grid = build_walk_grid(lvl);
    CHECK_FALSE(grid.is_walkable(0, 0));
    CHECK(grid.is_walkable(1, 0));
    CHECK_FALSE(grid.is_walkable(0, 1));
    CHECK(grid.is_walkable(1, 1));
}

TEST_CASE("walk grid stores chunks column-major") {
    gl::assets::LvlData lvl;
    lvl.chunk_width = 2;
    lvl.chunk_height = 2;
    lvl.chunks.resize(4);
    // File order is column-major: index = cx * chunk_height + cy.
    lvl.chunks[0][0].relief = 0x0000; // (cx=0, cy=0)
    lvl.chunks[1][0].relief = 0x0100; // (cx=0, cy=1) -> world (0, 2)
    lvl.chunks[2][0].relief = 0x0000; // (cx=1, cy=0)
    lvl.chunks[3][0].relief = 0x0000; // (cx=1, cy=1)

    const WalkGrid grid = build_walk_grid(lvl);
    CHECK(grid.width == 4);
    CHECK(grid.height == 4);
    CHECK_FALSE(grid.is_walkable(0, 2));
    CHECK(grid.is_walkable(2, 0));
    CHECK(grid.is_walkable(1, 2));
    CHECK(grid.is_walkable(0, 3));
    CHECK(grid.is_walkable(3, 3));
}

TEST_CASE("walk grid handles second chunk row") {
    gl::assets::LvlData lvl;
    lvl.chunk_width = 1;
    lvl.chunk_height = 2;
    lvl.chunks.resize(2);
    lvl.chunks[0][0].relief = 0x0000;
    lvl.chunks[1][0].relief = 0x0A00;

    const WalkGrid grid = build_walk_grid(lvl);
    CHECK(grid.width == 2);
    CHECK(grid.height == 4);
    CHECK(grid.is_walkable(0, 2));
    CHECK_FALSE(grid.is_walkable(1, 2));
    CHECK(grid.is_walkable(0, 3));
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
        if (value == 0)
            ++blocked;
    CHECK(blocked == 18028);
}
