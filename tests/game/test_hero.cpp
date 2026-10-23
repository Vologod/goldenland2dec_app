#include <doctest/doctest.h>
#include "gl/assets/pad.hpp"
#include "gl/game/hero.hpp"
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"

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

TEST_CASE("hero advances along a goal path and clears it on arrival") {
    gl::game::Level level;
    level.walkable.width = 8;
    level.walkable.height = 8;
    level.walkable.walkable.assign(64, 1);
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 2});
    const float start_x = hero.hero().x;
    REQUIRE(hero.set_goal({3, 2}));
    REQUIRE_FALSE(hero.hero().path.empty());
    hero.advance(0.1);
    CHECK(hero.hero().x > start_x);
    CHECK(hero.hero().direction == 6);
    REQUIRE_FALSE(hero.hero().path.empty());
    hero.advance(2.0);
    CHECK(hero.hero().tile == gl::game::Tile{3, 2});
    CHECK(hero.hero().direction == 6);
    CHECK(hero.hero().path.empty());
    CHECK_FALSE(hero.hero().has_goal);
}

TEST_CASE("set_goal rejects blocked and out-of-bounds targets") {
    gl::game::Level level;
    level.walkable.width = 8;
    level.walkable.height = 8;
    level.walkable.walkable.assign(64, 1);
    level.walkable.walkable[2 * 8 + 7] = 0;
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 2});
    CHECK_FALSE(hero.set_goal({8, 2}));
    CHECK_FALSE(hero.set_goal({-1, 2}));
    CHECK_FALSE(hero.set_goal({7, 2}));
    CHECK(hero.hero().path.empty());
    CHECK_FALSE(hero.hero().has_goal);
}

TEST_CASE("hero paths to a free cell next to npc") {
    gl::game::Level level;
    level.walkable.width = 8;
    level.walkable.height = 8;
    level.walkable.walkable.assign(64, 1);
    gl::game::PersonInstance npc;
    npc.tile_x = 2;
    npc.tile_y = 2;
    npc.route_type = "stay";
    level.persons.push_back(npc);
    const gl::game::LevelSimulation simulation = gl::game::LevelSimulation::create(level, 1);
    gl::game::HeroController hero(level, &simulation);
    hero.spawn_at_tile({0, 2});
    CHECK(hero.set_goal({2, 2}));
    CHECK_FALSE(hero.hero().path.empty());
    CHECK(hero.hero().path.back() != gl::game::Tile{2, 2});
    CHECK(hero.hero().path.back().y == 2);
}

TEST_CASE("rejected set_goal clears the previous goal") {
    gl::game::Level level;
    level.walkable.width = 8;
    level.walkable.height = 8;
    level.walkable.walkable.assign(64, 1);
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 2});
    REQUIRE(hero.set_goal({3, 2}));
    CHECK(hero.hero().goal == gl::game::Tile{3, 2});
    CHECK_FALSE(hero.set_goal({8, 2}));
    CHECK_FALSE(hero.hero().has_goal);
    CHECK(hero.hero().goal == hero.hero().tile);
    CHECK(hero.hero().path.empty());
}

TEST_CASE("can_interact uses npc frame extents") {
    gl::game::PersonInstance p;
    p.stay_type = gl::assets::PadAnimationType::rt_stay;
    gl::assets::PadAnimation a;
    a.frame_width = 40;
    a.frame_height = 60;
    a.anchor_x = 20;
    a.anchor_y = 55;
    p.pad.push_back(a);
    gl::game::PersonMovement m;
    m.x = 100;
    m.y = 100;
    gl::game::Level level;
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 0});
    hero.set_hero_world(125.0f, 100.0f);
    CHECK(hero.can_interact(p, m));
}

TEST_CASE("can_interact is false when the marker is outside the npc frame") {
    gl::game::PersonInstance p;
    p.stay_type = gl::assets::PadAnimationType::rt_stay;
    gl::assets::PadAnimation a;
    a.frame_width = 40;
    a.frame_height = 60;
    a.anchor_x = 20;
    a.anchor_y = 55;
    p.pad.push_back(a);
    gl::game::PersonMovement m;
    m.x = 100;
    m.y = 100;
    gl::game::Level level;
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 0});
    hero.set_hero_world(100.0f, 20.0f);
    CHECK_FALSE(hero.can_interact(p, m));
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
