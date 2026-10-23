#include <doctest/doctest.h>
#include "gl/game/hero.hpp"
#include "gl/game/level.hpp"

TEST_CASE("tile_at round-trips tile centers") {
    using namespace gl::game;
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 50; ++x) {
            const Tile t = tile_at(tile_center_x(x), tile_center_y(y));
            CHECK(t.x == x);
            CHECK(t.y == y);
        }
    }
}

TEST_CASE("hero spawns at first walkable entrance") {
    gl::game::Level level;
    level.walkable.width = 10;
    level.walkable.height = 10;
    level.walkable.walkable.assign(100, 1);
    level.entrances.push_back({"blocked", 1, 1, 0});
    level.entrances.push_back({"free", 2, 2, 0});
    level.walkable.walkable[1 * 10 + 1] = 0;
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_first_entrance();
    CHECK(hero.hero().tile.x == 2);
    CHECK(hero.hero().tile.y == 2);
}
