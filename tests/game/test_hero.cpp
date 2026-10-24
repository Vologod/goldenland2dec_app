#include <doctest/doctest.h>
#include <algorithm>
#include <cstdlib>
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
    REQUIRE_FALSE(hero.hero().path.empty());
    const gl::game::Tile end = hero.hero().path.back();
    CHECK(end != gl::game::Tile{2, 2});
    CHECK(end != gl::game::Tile{0, 2});
    CHECK(std::max(std::abs(end.x - 2), std::abs(end.y - 2)) == 1);
    CHECK(std::find(hero.hero().path.begin(), hero.hero().path.end(),
                    gl::game::Tile{2, 2}) == hero.hero().path.end());
    CHECK(end.y == 2);
}

TEST_CASE("set_goal rejects a non-walkable goal instead of falling back") {
    gl::game::Level level;
    level.walkable.width = 8;
    level.walkable.height = 8;
    level.walkable.walkable.assign(64, 1);
    level.walkable.walkable[2 * 8 + 3] = 0;
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 2});
    CHECK_FALSE(hero.set_goal({3, 2}));
    CHECK_FALSE(hero.hero().has_goal);
    CHECK(hero.hero().path.empty());
    CHECK(hero.hero().goal == hero.hero().tile);
}

TEST_CASE("set_goal rejects an unreachable walkable goal without a neighbor fallback") {
    gl::game::Level level;
    level.walkable.width = 10;
    level.walkable.height = 10;
    level.walkable.walkable.assign(100, 1);
    for (std::int32_t y = 4; y <= 6; ++y) {
        for (std::int32_t x = 4; x <= 6; ++x) {
            if (x == 5 && y == 5) continue;
            level.walkable.walkable[static_cast<std::size_t>(y) * 10 +
                                    static_cast<std::size_t>(x)] = 0;
        }
    }
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 5});
    CHECK_FALSE(hero.set_goal({5, 5}));
    CHECK_FALSE(hero.hero().has_goal);
    CHECK(hero.hero().path.empty());
    CHECK(hero.hero().goal == hero.hero().tile);
}

TEST_CASE("clear_goal stops the hero without teleporting") {
    gl::game::Level level;
    level.walkable.width = 8;
    level.walkable.height = 8;
    level.walkable.walkable.assign(64, 1);
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 2});
    REQUIRE(hero.set_goal({4, 2}));
    hero.advance(0.1);
    const gl::game::Tile position = hero.hero().tile;
    hero.clear_goal();
    CHECK_FALSE(hero.hero().has_goal);
    CHECK(hero.hero().path.empty());
    CHECK(hero.hero().goal == position);
    CHECK(hero.hero().tile == position);
}

TEST_CASE("retargeting a moved npc uses its current simulation tile") {
    gl::game::Level level;
    level.walkable.width = 10;
    level.walkable.height = 10;
    level.walkable.walkable.assign(100, 1);
    gl::game::PersonInstance npc;
    npc.tile_x = 2;
    npc.tile_y = 2;
    npc.route_type = "stay";
    level.persons.push_back(npc);
    gl::game::LevelSimulation simulation = gl::game::LevelSimulation::create(level, 1);
    gl::game::HeroController hero(level, &simulation);
    hero.spawn_at_tile({0, 2});
    REQUIRE(hero.set_goal({2, 2}));
    const gl::game::Tile old_goal = hero.hero().goal;

    simulation.place_person(0, {7, 7});
    REQUIRE(hero.set_goal({7, 7}));
    CHECK(hero.hero().goal != old_goal);
    CHECK(hero.hero().goal != gl::game::Tile{7, 7});
    CHECK(std::max(std::abs(hero.hero().goal.x - 7),
                   std::abs(hero.hero().goal.y - 7)) == 1);
    CHECK(std::find(hero.hero().path.begin(), hero.hero().path.end(),
                    gl::game::Tile{7, 7}) == hero.hero().path.end());
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

TEST_CASE("can_interact applies npc anchor offsets") {
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
    hero.set_hero_world(85.0f, 60.0f);
    CHECK(hero.can_interact(p, m));
}

TEST_CASE("can_interact is false without a matching stay animation") {
    gl::game::PersonInstance p;
    p.stay_type = gl::assets::PadAnimationType::rt_stay;
    gl::assets::PadAnimation a;
    a.type = gl::assets::PadAnimationType::tb_stay;
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
    hero.set_hero_world(100.0f, 100.0f);
    CHECK_FALSE(hero.can_interact(p, m));
}

TEST_CASE("can_interact is false for a zero-size npc frame") {
    gl::game::PersonInstance p;
    p.stay_type = gl::assets::PadAnimationType::rt_stay;
    gl::assets::PadAnimation a;
    a.frame_width = 0;
    a.frame_height = 0;
    a.anchor_x = 0;
    a.anchor_y = 0;
    p.pad.push_back(a);
    gl::game::PersonMovement m;
    m.x = 100;
    m.y = 100;
    gl::game::Level level;
    gl::game::HeroController hero(level, nullptr);
    hero.spawn_at_tile({0, 0});
    hero.set_hero_world(100.0f, 100.0f);
    CHECK_FALSE(hero.can_interact(p, m));
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
