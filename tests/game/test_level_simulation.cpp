#include "gl/game/level_simulation.hpp"
#include <cmath>
#include <doctest/doctest.h>
#include <utility>

using namespace gl::game;

namespace {
Level make_level(std::string route_type, std::vector<std::pair<std::int32_t, std::int32_t>> cells,
                 std::int32_t delay_min, std::int32_t delay_max) {
    Level level;
    level.name = "test";
    level.width = 240 * 12;
    level.height = 240 * 9;
    PersonInstance person;
    person.name = "person";
    person.tile_x = 10;
    person.tile_y = 10;
    person.route_type = std::move(route_type);
    person.radius = 3;
    person.delay_min = delay_min;
    person.delay_max = delay_max;
    person.route_cells = std::move(cells);
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(std::move(person));
    return level;
}
} // namespace

TEST_CASE("MOVED follows route cells in order") {
    Level level = make_level("MOVED", {{10, 10}, {13, 10}, {13, 13}}, 0, 0);
    auto sim = LevelSimulation::create(level, 1);
    CHECK(sim.can_move(0));
    for (int i = 0; i < 600; ++i)
        sim.advance(1.0 / 60.0);
    CHECK(sim.persons()[0].x > 10.0f * 12 + 6);
}

TEST_CASE("delay under 500 ms does not stop") {
    Level level = make_level("MOVED", {{10, 10}, {13, 10}}, 0, 400);
    auto sim = LevelSimulation::create(level, 1);
    sim.advance(3.0);
    CHECK(sim.persons()[0].x == doctest::Approx(13.0f * 12 + 6).epsilon(0.01));
}

TEST_CASE("delay over 500 ms waits") {
    Level level = make_level("MOVED", {{10, 10}, {13, 10}}, 1000, 1000);
    auto sim = LevelSimulation::create(level, 1);
    sim.advance(0.5);
    CHECK(sim.persons()[0].x < 10.0f * 12 + 6 + 1.0f);
    sim.advance(2.0);
    CHECK(sim.persons()[0].x > 10.0f * 12 + 6 + 4.0f);
}

TEST_CASE("RANDOM_RADIUS stays within radius") {
    Level level = make_level("RANDOM_RADIUS", {}, 0, 0);
    auto sim = LevelSimulation::create(level, 3);
    for (int i = 0; i < 600; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        CHECK(std::fabs(p.x - (10.0f * 12 + 6)) <= 3 * 12 + 6);
        CHECK(std::fabs(p.y - (10.0f * 9 + 4)) <= 3 * 9 + 4);
    }
}

TEST_CASE("simulation is deterministic") {
    Level level = make_level("MOVED_FLIP", {{10, 10}, {13, 10}, {13, 13}, {10, 13}}, 0, 900);
    auto a = LevelSimulation::create(level, 7);
    auto b = LevelSimulation::create(level, 7);
    for (int i = 0; i < 120; ++i) {
        a.advance(1.0 / 60.0);
        b.advance(1.0 / 60.0);
    }
    CHECK(a.persons()[0].x == b.persons()[0].x);
    CHECK(a.persons()[0].y == b.persons()[0].y);
}

TEST_CASE("person without go cannot move") {
    Level level = make_level("MOVED", {{10, 10}, {13, 10}}, 0, 0);
    level.persons[0].go_type.reset();
    auto sim = LevelSimulation::create(level, 1);
    CHECK_FALSE(sim.can_move(0));
    sim.advance(5.0);
    CHECK(sim.persons()[0].x == doctest::Approx(10.0f * 12 + 6));
}
