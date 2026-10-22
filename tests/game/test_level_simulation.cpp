#include "gl/game/level_simulation.hpp"
#include <algorithm>
#include <cmath>
#include <doctest/doctest.h>
#include <utility>
#include <vector>

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
    float max_x = sim.persons()[0].x;
    for (int i = 0; i < 180; ++i) {
        sim.advance(1.0 / 60.0);
        max_x = std::max(max_x, sim.persons()[0].x);
    }
    CHECK(max_x == doctest::Approx(13.0f * 12 + 6).epsilon(0.01));
    CHECK_FALSE(sim.persons()[0].waiting);
}

TEST_CASE("4 px arrival snaps to target") {
    Level level = make_level("MOVED", {{10, 10}, {13, 10}}, 0, 0);
    auto sim = LevelSimulation::create(level, 1);
    for (int i = 0; i < 33; ++i)
        sim.advance(1.0 / 60.0);
    const auto &before = sim.persons()[0];
    CHECK(before.x == doctest::Approx(13.0f * 12 + 6 - 3.0f));
    CHECK(before.x < 13.0f * 12 + 6);
    sim.advance(1.0 / 60.0);
    CHECK(sim.persons()[0].x == doctest::Approx(13.0f * 12 + 6));
}

TEST_CASE("delay_min above delay_max is swapped") {
    Level level = make_level("MOVED", {{10, 10}, {13, 10}}, 600, 100);
    auto sim = LevelSimulation::create(level, 1);
    const auto &p = sim.persons()[0];
    CHECK_FALSE(p.waiting);
    CHECK(p.moving);
}

TEST_CASE("MOVED cycles route cells in order") {
    Level level = make_level("MOVED", {{11, 10}, {14, 10}, {14, 13}}, 0, 0);
    auto sim = LevelSimulation::create(level, 1);
    std::vector<std::pair<float, float>> targets;
    const auto &first = sim.persons()[0];
    targets.emplace_back(first.target_x, first.target_y);
    for (int i = 0; i < 400 && targets.size() < 4; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        if (p.target_x != targets.back().first || p.target_y != targets.back().second)
            targets.emplace_back(p.target_x, p.target_y);
    }
    REQUIRE(targets.size() == 4);
    CHECK(targets[0].first == doctest::Approx(11.0f * 12 + 6));
    CHECK(targets[0].second == doctest::Approx(10.0f * 9 + 4));
    CHECK(targets[1].first == doctest::Approx(14.0f * 12 + 6));
    CHECK(targets[1].second == doctest::Approx(10.0f * 9 + 4));
    CHECK(targets[2].first == doctest::Approx(14.0f * 12 + 6));
    CHECK(targets[2].second == doctest::Approx(13.0f * 9 + 4));
    CHECK(targets[3].first == doctest::Approx(targets[0].first));
    CHECK(targets[3].second == doctest::Approx(targets[0].second));
}

TEST_CASE("MOVED_FLIP walks one route step with fixed seed") {
    Level level = make_level("MOVED_FLIP", {{11, 10}, {14, 10}, {14, 13}}, 0, 0);
    auto plus = LevelSimulation::create(level, 1);
    auto minus = LevelSimulation::create(level, 2);
    for (int i = 0; i < 10; ++i) {
        plus.advance(1.0 / 60.0);
        minus.advance(1.0 / 60.0);
    }
    CHECK(plus.persons()[0].target_x == doctest::Approx(14.0f * 12 + 6));
    CHECK(plus.persons()[0].target_y == doctest::Approx(10.0f * 9 + 4));
    CHECK(minus.persons()[0].target_x == doctest::Approx(11.0f * 12 + 6));
    CHECK(minus.persons()[0].target_y == doctest::Approx(10.0f * 9 + 4));
}

TEST_CASE("RANDOM samples around current tile") {
    Level level = make_level("RANDOM", {}, 0, 0);
    level.persons[0].radius = 1;
    auto sim = LevelSimulation::create(level, 11);
    const auto tile = [](float value, float size) {
        return static_cast<std::int32_t>(std::floor(value / size));
    };
    const auto &start = sim.persons()[0];
    CHECK(std::abs(tile(start.target_x, 12.0f) - tile(start.x, 12.0f)) <= 1);
    CHECK(std::abs(tile(start.target_y, 9.0f) - tile(start.y, 9.0f)) <= 1);
    float last_x = start.target_x;
    float last_y = start.target_y;
    int samples = 0;
    for (int i = 0; i < 4000; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        if (p.target_x == last_x && p.target_y == last_y)
            continue;
        CHECK(std::abs(tile(p.target_x, 12.0f) - tile(p.x, 12.0f)) <= 1);
        CHECK(std::abs(tile(p.target_y, 9.0f) - tile(p.y, 9.0f)) <= 1);
        last_x = p.target_x;
        last_y = p.target_y;
        ++samples;
    }
    CHECK(samples > 10);
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
