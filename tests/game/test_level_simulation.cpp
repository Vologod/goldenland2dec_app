#include "gl/assets/vfs.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/msvc_random.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <doctest/doctest.h>
#include <utility>
#include <vector>

using namespace gl::game;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

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

Level make_walled_level() {
    Level level;
    level.name = "wall";
    level.width = 14 * 12;
    level.height = 10 * 9;
    level.walkable.width = 14;
    level.walkable.height = 10;
    level.walkable.walkable.assign(14 * 10, 1);
    for (int y = 0; y < 8; ++y)
        level.walkable.walkable[static_cast<std::size_t>(y) * 14 + 7] = 0;
    PersonInstance person;
    person.name = "walker";
    person.tile_x = 2;
    person.tile_y = 2;
    person.route_type = "MOVED";
    person.route_cells = {{12, 2}};
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(person);
    return level;
}
} // namespace

TEST_CASE("initial planning rolls delay before selecting a RANDOM target") {
    Level level = make_level("RANDOM", {}, 0, 1000);
    level.persons[0].radius = 3;
    auto sim = LevelSimulation::create(level, 1);

    MsvcRandom rng(1);
    const std::int32_t delay = rng.range(0, 1000);
    REQUIRE(delay == 1);
    const std::int32_t offset_x = rng.range(-3, 3);
    const std::int32_t offset_y = rng.range(-3, 3);
    CHECK(offset_x == 1);
    CHECK(offset_y == -2);

    const auto tile = [](float value, float size) {
        return static_cast<std::int32_t>(std::floor(value / size));
    };
    const auto &p = sim.persons()[0];
    CHECK_FALSE(p.waiting);
    CHECK(p.moving);
    CHECK(tile(p.target_x, 12.0f) == 10 + offset_x);
    CHECK(tile(p.target_y, 9.0f) == 10 + offset_y);
}

TEST_CASE("initial planning waits when the first delay is 500 ms or more") {
    Level level = make_level("MOVED", {{11, 10}, {13, 10}}, 1000, 1000);
    auto sim = LevelSimulation::create(level, 1);
    const auto &p = sim.persons()[0];
    CHECK(p.waiting);
    CHECK_FALSE(p.moving);
    CHECK(p.wait_until_ms == 1000u);
    CHECK(p.path.empty());
    CHECK(p.x == doctest::Approx(10.0f * 12 + 6));
}

TEST_CASE("initial MOVED_FLIP planning rolls the delay before the coin") {
    Level level = make_level("MOVED_FLIP", {{11, 10}, {14, 10}, {14, 13}}, 600, 700);
    auto sim = LevelSimulation::create(level, 1);

    MsvcRandom rng(1);
    CHECK(rng.range(600, 700) == 600);

    const auto &p = sim.persons()[0];
    CHECK(p.waiting);
    CHECK_FALSE(p.moving);
    CHECK(p.wait_until_ms == 600u);
    CHECK(p.path.empty());
}

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
    auto minus = LevelSimulation::create(level, 5006);
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
    auto sim = LevelSimulation::create(level, 1);
    const auto tile = [](float value, float size) {
        return static_cast<std::int32_t>(std::floor(value / size));
    };
    const std::int32_t max_offset = level.persons[0].radius + 1;
    const auto &start = sim.persons()[0];
    CHECK(std::abs(tile(start.target_x, 12.0f) - tile(start.x, 12.0f)) <= max_offset);
    CHECK(std::abs(tile(start.target_y, 9.0f) - tile(start.y, 9.0f)) <= max_offset);
    float last_x = start.target_x;
    float last_y = start.target_y;
    int samples = 0;
    for (int i = 0; i < 4000; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        if (p.target_x == last_x && p.target_y == last_y)
            continue;
        CHECK(std::abs(tile(p.target_x, 12.0f) - tile(p.x, 12.0f)) <= max_offset);
        CHECK(std::abs(tile(p.target_y, 9.0f) - tile(p.y, 9.0f)) <= max_offset);
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

TEST_CASE("RANDOM_RADIUS stays within radius plus one") {
    Level level = make_level("RANDOM_RADIUS", {}, 0, 0);
    auto sim = LevelSimulation::create(level, 1);
    const std::int32_t max_offset = level.persons[0].radius + 1;
    for (int i = 0; i < 600; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        CHECK(std::fabs(p.x - (10.0f * 12 + 6)) <= max_offset * 12);
        CHECK(std::fabs(p.y - (10.0f * 9 + 4)) <= max_offset * 9);
    }
}

TEST_CASE("RANDOM_RADIUS only targets walkable in-bounds tiles") {
    Level level;
    level.name = "obstacles";
    level.width = 40 * 12;
    level.height = 40 * 9;
    level.walkable.width = 40;
    level.walkable.height = 40;
    level.walkable.walkable.assign(40 * 40, 1);
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 40; ++x) {
            if ((x * 3 + y * 5) % 11 == 0)
                level.walkable.walkable[static_cast<std::size_t>(y) * 40 + x] = 0;
        }
    }
    PersonInstance person;
    person.name = "random_radius_walker";
    person.tile_x = 20;
    person.tile_y = 20;
    person.route_type = "RANDOM_RADIUS";
    person.radius = 6;
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(person);

    auto sim = LevelSimulation::create(level, 1);
    std::vector<Tile> goals;
    for (int i = 0; i < 60 * 120; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        if (p.path.size() < 2)
            continue;
        const Tile goal = p.path.back();
        CHECK(goal.x >= 0);
        CHECK(goal.y >= 0);
        CHECK(goal.x < level.walkable.width);
        CHECK(goal.y < level.walkable.height);
        CHECK(level.walkable.is_walkable(goal.x, goal.y));
        if (goals.empty() || goals.back() != goal)
            goals.push_back(goal);
    }
    CHECK(goals.size() > 3);
}

TEST_CASE("RANDOM_RADIUS may select a tile occupied by another person") {
    Level level;
    level.name = "occupied";
    level.width = 30 * 12;
    level.height = 30 * 9;
    level.walkable.width = 30;
    level.walkable.height = 30;
    level.walkable.walkable.assign(30 * 30, 1);
    PersonInstance walker;
    walker.name = "random_radius_walker";
    walker.tile_x = 10;
    walker.tile_y = 10;
    walker.route_type = "RANDOM_RADIUS";
    walker.radius = 1;
    walker.go_type = gl::assets::PadAnimationType::rt_go;
    walker.go_move_x = 6.0f;
    walker.go_move_y = 5.0f;
    walker.go_delay_ms = 100;
    level.persons.push_back(walker);
    PersonInstance blocker = walker;
    blocker.name = "blocker";
    blocker.tile_x = 11;
    blocker.tile_y = 10;
    blocker.route_type = "STAY";
    blocker.route_cells.clear();
    level.persons.push_back(blocker);

    auto sim = LevelSimulation::create(level, 1);
    bool saw_occupied_goal = false;
    for (int i = 0; i < 60 * 120 && !saw_occupied_goal; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        if (p.path.size() >= 2 && p.path.back() == Tile{11, 10})
            saw_occupied_goal = true;
    }
    CHECK(saw_occupied_goal);
}

TEST_CASE("RANDOM_RADIUS can repeat the same candidate") {
    Level level;
    level.name = "repeat";
    level.width = 30 * 12;
    level.height = 30 * 9;
    level.walkable.width = 30;
    level.walkable.height = 30;
    level.walkable.walkable.assign(30 * 30, 1);
    PersonInstance person;
    person.name = "random_radius_walker";
    person.tile_x = 10;
    person.tile_y = 10;
    person.route_type = "RANDOM_RADIUS";
    person.radius = 1;
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(person);

    auto sim = LevelSimulation::create(level, 1);
    std::vector<Tile> goals;
    for (int i = 0; i < 60 * 180; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        if (p.path.size() < 2)
            continue;
        const Tile goal = p.path.back();
        if (goals.empty() || goals.back() != goal)
            goals.push_back(goal);
    }
    REQUIRE(goals.size() > 9);
    bool saw_repeat = false;
    for (std::size_t a = 0; a < goals.size() && !saw_repeat; ++a) {
        for (std::size_t b = a + 1; b < goals.size(); ++b) {
            if (goals[a] == goals[b]) {
                saw_repeat = true;
                break;
            }
        }
    }
    CHECK(saw_repeat);
}

TEST_CASE("RANDOM with out-of-bounds base keeps the base tile") {
    Level level = make_level("RANDOM", {}, 0, 0);
    level.persons[0].tile_x = -2;
    level.persons[0].tile_y = 10;
    auto sim = LevelSimulation::create(level, 1);
    const auto &p = sim.persons()[0];
    CHECK(p.x == doctest::Approx(-2.0f * 12 + 6));
    CHECK(p.target_x == doctest::Approx(-2.0f * 12 + 6));
    CHECK(p.target_y == doctest::Approx(10.0f * 9 + 4));
    REQUIRE(p.path.size() >= 2);
    CHECK(p.path.back() == Tile{-2, 10});
}

TEST_CASE("failed RANDOM plan resamples on the next attempt") {
    Level level;
    level.name = "split";
    level.width = 30 * 12;
    level.height = 30 * 9;
    level.walkable.width = 30;
    level.walkable.height = 30;
    level.walkable.walkable.assign(30 * 30, 0);
    for (int y = 0; y < 30; ++y)
        for (int x = 0; x < 30; ++x)
            level.walkable.walkable[static_cast<std::size_t>(y) * 30 + x] =
                (x <= 9 || x >= 14) ? 1 : 0;
    PersonInstance person;
    person.name = "random_walker";
    person.tile_x = 4;
    person.tile_y = 15;
    person.route_type = "RANDOM";
    person.radius = 20;
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(person);

    const auto sample_goal = [&](MsvcRandom &rng) {
        for (;;) {
            const Tile candidate{4 + rng.range(-20, 20), 15 + rng.range(-20, 20)};
            if (candidate.x < 0 || candidate.y < 0 || candidate.x >= 30 || candidate.y >= 30)
                continue;
            if (!level.walkable.is_walkable(candidate.x, candidate.y))
                continue;
            return candidate;
        }
    };
    std::uint64_t seed = 0;
    Tile expected{};
    for (std::uint64_t candidate_seed = 1;
         candidate_seed <= 20000 && seed == 0; ++candidate_seed) {
        MsvcRandom rng(static_cast<std::uint32_t>(candidate_seed));
        const Tile first = sample_goal(rng);
        if (first.x <= 9)
            continue;
        const Tile second = sample_goal(rng);
        if (second.x >= 14)
            continue;
        seed = candidate_seed;
        expected = second;
    }
    REQUIRE(seed != 0);
    auto sim = LevelSimulation::create(level, seed);
    for (int i = 0; i < 40; ++i)
        sim.advance(1.0 / 60.0);
    const auto &p = sim.persons()[0];
    CHECK(p.moving);
    REQUIRE(p.path.size() >= 2);
    CHECK(p.path.back() == expected);
}

TEST_CASE("simulation is deterministic") {
    Level level = make_level("MOVED_FLIP", {{10, 10}, {13, 10}, {13, 13}, {10, 13}}, 0, 900);
    auto a = LevelSimulation::create(level, 1);
    auto b = LevelSimulation::create(level, 1);
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

TEST_CASE("walker goes around a wall and never enters blocked tiles") {
    auto sim = LevelSimulation::create(make_walled_level(), 1);
    for (int i = 0; i < 60 * 60; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        const int tile_x = static_cast<int>((p.x - 6.0f) / 12.0f + 0.5f);
        const int tile_y = static_cast<int>((p.y - 4.0f) / 9.0f + 0.5f);
        CHECK(make_walled_level().walkable.is_walkable(tile_x, tile_y));
    }
    CHECK(sim.persons()[0].x > 11 * 12);
}

TEST_CASE("clear line of sight walks straight without cell-center stops") {
    Level level;
    level.name = "open";
    level.width = 50 * 12;
    level.height = 50 * 9;
    level.walkable.width = 50;
    level.walkable.height = 50;
    level.walkable.walkable.assign(50 * 50, 1);
    PersonInstance person;
    person.name = "straight";
    person.tile_x = 5;
    person.tile_y = 5;
    person.route_type = "MOVED";
    person.route_cells = {{35, 28}};
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(person);

    auto sim = LevelSimulation::create(level, 1);
    const auto &start = sim.persons()[0];
    const float sx = start.x;
    const float sy = start.y;
    const float tx = 35.0f * 12.0f + 6.0f;
    const float ty = 28.0f * 9.0f + 4.0f;
    const float line_dx = tx - sx;
    const float line_dy = ty - sy;
    const float line_length = std::sqrt(line_dx * line_dx + line_dy * line_dy);
    REQUIRE(line_length > 0.0f);
    int moving_ticks = 0;
    for (int i = 0; i < 60 * 60; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        if (!p.moving)
            continue;
        ++moving_ticks;
        REQUIRE(p.path.size() == 2);
        CHECK(p.path.back() == Tile{35, 28});
        const float cross = (p.x - sx) * line_dy - (p.y - sy) * line_dx;
        CHECK(std::fabs(cross) / line_length < 0.01f);
    }
    CHECK(moving_ticks > 0);
    CHECK(sim.persons()[0].x == doctest::Approx(tx).epsilon(0.001));
    CHECK(sim.persons()[0].y == doctest::Approx(ty).epsilon(0.001));
}

TEST_CASE("two walkers never occupy the same tile") {
    Level level = make_walled_level();
    PersonInstance second = level.persons[0];
    second.name = "walker2";
    second.tile_x = 12;
    second.tile_y = 2;
    second.route_cells = {{2, 2}};
    level.persons.push_back(second);
    auto sim = LevelSimulation::create(level, 1);
    for (int i = 0; i < 60 * 30; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &a = sim.persons()[0];
        const auto &b = sim.persons()[1];
        const int ax = static_cast<int>((a.x - 6.0f) / 12.0f + 0.5f);
        const int ay = static_cast<int>((a.y - 4.0f) / 9.0f + 0.5f);
        const int bx = static_cast<int>((b.x - 6.0f) / 12.0f + 0.5f);
        const int by = static_cast<int>((b.y - 4.0f) / 9.0f + 0.5f);
        CHECK_FALSE((ax == bx && ay == by));
    }
}

TEST_CASE("blocked MOVED route cell is skipped after failed planning") {
    Level level = make_walled_level();
    level.persons[0].route_cells = {{7, 2}, {12, 2}};
    level.persons[0].delay_min = 0;
    level.persons[0].delay_max = 0;
    auto sim = LevelSimulation::create(level, 1);
    CHECK(sim.persons()[0].waiting);
    bool saw_path = false;
    bool reached = false;
    for (int i = 0; i < 60 * 10 && !reached; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[0];
        saw_path = saw_path || p.path.size() >= 2;
        reached =
            std::fabs(p.x - (12.0f * 12 + 6)) < 0.01f && std::fabs(p.y - (2.0f * 9 + 4)) < 0.01f;
    }
    CHECK(saw_path);
    CHECK(reached);
}

TEST_CASE("two walkers crossing with clear LOS never share a tile") {
    Level level;
    level.name = "cross";
    level.width = 45 * 12;
    level.height = 45 * 9;
    level.walkable.width = 45;
    level.walkable.height = 45;
    level.walkable.walkable.assign(45 * 45, 1);
    PersonInstance horizontal;
    horizontal.name = "horizontal";
    horizontal.tile_x = 4;
    horizontal.tile_y = 20;
    horizontal.route_type = "MOVED";
    horizontal.route_cells = {{40, 20}};
    horizontal.go_type = gl::assets::PadAnimationType::rt_go;
    horizontal.go_move_x = 6.0f;
    horizontal.go_move_y = 5.0f;
    horizontal.go_delay_ms = 100;
    level.persons.push_back(horizontal);
    PersonInstance vertical = horizontal;
    vertical.name = "vertical";
    vertical.tile_x = 20;
    vertical.tile_y = 2;
    vertical.route_cells = {{20, 42}};
    level.persons.push_back(vertical);
    auto sim = LevelSimulation::create(level, 1);
    for (int i = 0; i < 60 * 12; ++i) {
        sim.advance(1.0 / 60.0);
        const auto &a = sim.persons()[0];
        const auto &b = sim.persons()[1];
        const int ax = static_cast<int>((a.x - 6.0f) / 12.0f + 0.5f);
        const int ay = static_cast<int>((a.y - 4.0f) / 9.0f + 0.5f);
        const int bx = static_cast<int>((b.x - 6.0f) / 12.0f + 0.5f);
        const int by = static_cast<int>((b.y - 4.0f) / 9.0f + 0.5f);
        CHECK_FALSE((ax == bx && ay == by));
    }
    CHECK(sim.persons()[0].x > 20 * 12);
    CHECK(sim.persons()[1].y > 20 * 9);
}

TEST_CASE("real data: l1_1 movers stay walkable and apart" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    auto sim = LevelSimulation::create(level.value(), 1);
    auto twin = LevelSimulation::create(level.value(), 1);
    std::vector<char> spawn_walkable(level->persons.size(), 0);
    for (std::size_t i = 0; i < level->persons.size(); ++i) {
        spawn_walkable[i] =
            level->walkable.is_walkable(level->persons[i].tile_x, level->persons[i].tile_y);
    }
    std::vector<char> saw_path(level->persons.size(), 0);
    int walkable_movers = 0;
    for (int tick = 0; tick < 60 * 60; ++tick) {
        sim.advance(1.0 / 60.0);
        twin.advance(1.0 / 60.0);
        const auto &persons = sim.persons();
        for (std::size_t a = 0; a < persons.size(); ++a) {
            const int ax = static_cast<int>((persons[a].x - 6.0f) / 12.0f + 0.5f);
            const int ay = static_cast<int>((persons[a].y - 4.0f) / 9.0f + 0.5f);
            if (sim.can_move(a) && spawn_walkable[a]) {
                CHECK(level->walkable.is_walkable(ax, ay));
                ++walkable_movers;
                if (persons[a].path.size() >= 2)
                    saw_path[a] = 1;
            }
            for (std::size_t b = a + 1; b < persons.size(); ++b) {
                const int bx = static_cast<int>((persons[b].x - 6.0f) / 12.0f + 0.5f);
                const int by = static_cast<int>((persons[b].y - 4.0f) / 9.0f + 0.5f);
                CHECK_FALSE((ax == bx && ay == by));
            }
        }
    }
    CHECK(walkable_movers > 0);
    int walkable_spawn_movers = 0;
    int walkable_spawn_movers_with_path = 0;
    for (std::size_t i = 0; i < sim.persons().size(); ++i) {
        if (!sim.can_move(i) || !spawn_walkable[i])
            continue;
        ++walkable_spawn_movers;
        walkable_spawn_movers_with_path += saw_path[i] ? 1 : 0;
    }
    CHECK(walkable_spawn_movers > 0);
    CAPTURE(walkable_spawn_movers_with_path);
    CHECK(walkable_spawn_movers_with_path > 0);
    for (std::size_t i = 0; i < sim.persons().size(); ++i) {
        CHECK(sim.persons()[i].x == twin.persons()[i].x);
        CHECK(sim.persons()[i].y == twin.persons()[i].y);
    }
}

TEST_CASE("real data: l1_1 oficer1 is not stuck on its first route cell" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    std::size_t oficer = level->persons.size();
    for (std::size_t i = 0; i < level->persons.size(); ++i) {
        if (level->persons[i].name == "L0.P744_oficer1")
            oficer = i;
    }
    REQUIRE(oficer < level->persons.size());
    auto sim = LevelSimulation::create(level.value(), 1);
    const int start_tile_x = static_cast<int>((sim.persons()[oficer].x - 6.0f) / 12.0f + 0.5f);
    const int start_tile_y = static_cast<int>((sim.persons()[oficer].y - 4.0f) / 9.0f + 0.5f);
    bool saw_path = false;
    bool changed_tile = false;
    for (int tick = 0; tick < 60 * 60; ++tick) {
        sim.advance(1.0 / 60.0);
        const auto &p = sim.persons()[oficer];
        saw_path = saw_path || p.path.size() >= 2;
        const int tile_x = static_cast<int>((p.x - 6.0f) / 12.0f + 0.5f);
        const int tile_y = static_cast<int>((p.y - 4.0f) / 9.0f + 0.5f);
        changed_tile = changed_tile || tile_x != start_tile_x || tile_y != start_tile_y;
    }
    CHECK(saw_path);
    CHECK(changed_tile);
}

TEST_CASE("RANDOM resamples after repeated unreachable goals") {
    Level level;
    level.name = "split";
    level.width = 30 * 12;
    level.height = 30 * 9;
    level.walkable.width = 30;
    level.walkable.height = 30;
    level.walkable.walkable.assign(30 * 30, 0);
    for (int y = 0; y < 30; ++y)
        for (int x = 0; x < 30; ++x)
            level.walkable.walkable[static_cast<std::size_t>(y) * 30 + x] =
                (x <= 9 || x >= 14) ? 1 : 0;
    PersonInstance person;
    person.name = "random_walker";
    person.tile_x = 4;
    person.tile_y = 15;
    person.route_type = "RANDOM";
    person.radius = 20;
    person.go_type = gl::assets::PadAnimationType::rt_go;
    person.go_move_x = 6.0f;
    person.go_move_y = 5.0f;
    person.go_delay_ms = 100;
    level.persons.push_back(person);

    std::uint64_t seed = 0;
    for (std::uint64_t candidate = 1; candidate <= 500 && seed == 0; ++candidate) {
        auto probe = LevelSimulation::create(level, candidate);
        if (probe.persons()[0].waiting)
            seed = candidate;
    }
    REQUIRE(seed != 0);
    auto sim = LevelSimulation::create(level, seed);
    bool saw_moving = false;
    float start_x = sim.persons()[0].x;
    for (int i = 0; i < 60 * 60; ++i) {
        sim.advance(1.0 / 60.0);
        saw_moving = saw_moving || sim.persons()[0].moving;
    }
    CHECK(saw_moving);
    CHECK(std::fabs(sim.persons()[0].x - start_x) > 1.0f);
}
