#include "gl/assets/pad.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/person.hpp"

#include <cstdlib>
#include <doctest/doctest.h>
#include <string>
#include <utility>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

const gl::assets::PadAnimation *find_stay(const gl::game::PersonInstance &person) {
    for (const auto &animation : person.pad) {
        if (animation.type == person.stay_type)
            return &animation;
    }
    return nullptr;
}
} // namespace

TEST_CASE("real data: l1_1 person loader builds 16 instances" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto persons = gl::game::load_persons(vfs.value(), "l1_1");
    REQUIRE(persons.has_value());
    REQUIRE(persons->size() == 16);

    const gl::game::PersonInstance &first = persons->front();
    CHECK(first.name == "L1_1.P387_Poison_Plotnik");
    CHECK(first.tile_x == 133);
    CHECK(first.tile_y == 12);
    CHECK(first.direction == 5);
    CHECK(first.route_type == "STAY");
    CHECK(first.radius == 15);
    CHECK(first.delay_min == 0);
    CHECK(first.delay_max == 9000);
    CHECK(first.tribe == "L1_1_citizen");
    CHECK(first.dialog == "l1_1\\l1_1.p387_poison_plotnik.d387.age");
    CHECK(first.inventory == "p387");
    CHECK(first.res_name == "L0.P1131_Inhabitans03");
    CHECK(first.def.res_name == "L0.P1131_Inhabitans03");

    const gl::assets::PadAnimation *first_stay = find_stay(first);
    REQUIRE(first_stay != nullptr);
    CHECK(first.stay_type == gl::assets::PadAnimationType::rt_stay);
    CHECK(first_stay->frames_per_row == 12);
    CHECK(first_stay->frame_width == 41);
    CHECK(first_stay->frame_height == 89);
    CHECK(first_stay->crops.size() == 5);
    CHECK(first.sprite.width == 492);
    CHECK(first.sprite.height == 445);
    CHECK(first.shadow.width == 588);
    CHECK(first.shadow.height == 232);

    const gl::game::PersonInstance &last = persons->back();
    CHECK(last.name == "L1.P620_Svet_3CircusWorker2");
    CHECK(last.tile_x == 173);
    CHECK(last.tile_y == 189);
    CHECK(last.direction == 3);
    CHECK(last.res_name == "L0.P1158_Clowns_Male01");

    std::size_t officers = 0;
    std::size_t turn_based = 0;
    for (const auto &person : persons.value()) {
        CHECK_FALSE(person.name.empty());
        CHECK_FALSE(person.res_name.empty());
        const gl::assets::PadAnimation *stay = find_stay(person);
        REQUIRE(stay != nullptr);
        CHECK(person.sprite.width ==
              static_cast<std::uint32_t>(stay->frame_width * stay->frames_per_row));
        CHECK(person.sprite.height ==
              static_cast<std::uint32_t>(stay->frame_height *
                                         static_cast<std::int32_t>(stay->crops.size())));
        if (person.name == "L0.P744_oficer1")
            ++officers;
        if (person.stay_type == gl::assets::PadAnimationType::tb_stay)
            ++turn_based;
    }
    CHECK(officers == 2);
    CHECK(turn_based == 6);
    CHECK(persons->at(10).tile_x == 66);
    CHECK(persons->at(10).tile_y == 136);
    CHECK(persons->at(11).tile_x == 190);
    CHECK(persons->at(11).tile_y == 148);

    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    REQUIRE(level->persons.size() == 16);
    CHECK(level->persons.front().name == "L1_1.P387_Poison_Plotnik");
}

TEST_CASE("real data: l1_1 routes and go animations" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "l1_1");
    REQUIRE(persons.has_value());

    int movers = 0;
    for (const auto &person : persons.value()) {
        if (person.route_type == "STAY") {
            CHECK(person.route_cells.empty());
            continue;
        }
        ++movers;
        if (person.route_type == "RANDOM_RADIUS" || person.route_type == "RANDOM") {
            CHECK(person.route_cells.empty());
        } else {
            CHECK(!person.route_cells.empty());
        }
        REQUIRE(person.go_type.has_value());
        CHECK(person.go_delay_ms > 0);
        CHECK(person.go_move_x > 0.0f);
        CHECK(person.go_move_y > 0.0f);
    }
    CHECK(movers == 6);
    CHECK(persons->at(1).route_cells.size() == 7);
    CHECK(persons->at(9).route_cells.size() == 9);
    CHECK(persons->at(11).route_cells.size() == 8);
    CHECK(persons->at(12).route_cells.size() == 22);
}

TEST_CASE("real data: l1_1 60 s simulation is bounded and deterministic" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "l1_1");
    REQUIRE(persons.has_value());
    REQUIRE(persons->size() == 16);

    gl::game::Level level;
    level.name = "l1_1";
    level.width = 2760;
    level.height = 2016;
    level.persons = std::move(persons.value());

    auto first = gl::game::LevelSimulation::create(level, 0x5EED);
    auto second = gl::game::LevelSimulation::create(level, 0x5EED);

    int movable = 0;
    int stationary = 0;
    int moved = 0;
    int moved_flip = 0;
    int random_radius = 0;
    for (std::size_t i = 0; i < level.persons.size(); ++i) {
        const gl::game::PersonInstance &person = level.persons[i];
        if (first.can_move(i))
            ++movable;
        else
            ++stationary;
        if (person.route_type == "MOVED") {
            ++moved;
            CHECK_FALSE(person.route_cells.empty());
        } else if (person.route_type == "MOVED_FLIP") {
            ++moved_flip;
            CHECK_FALSE(person.route_cells.empty());
        } else if (person.route_type == "RANDOM_RADIUS") {
            ++random_radius;
            CHECK(person.route_cells.empty());
        }
    }
    CHECK(stationary == 10);
    CHECK(movable == 6);
    CHECK(moved == 1);
    CHECK(moved_flip == 3);
    CHECK(random_radius == 2);

    bool bounded = true;
    bool identical = true;
    for (int step = 0; step < 60 * 60; ++step) {
        first.advance(1.0 / 60.0);
        second.advance(1.0 / 60.0);
        const auto &a = first.persons();
        const auto &b = second.persons();
        if (a.size() != b.size()) {
            identical = false;
            break;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i].x < 0.0f || a[i].x > static_cast<float>(level.width) || a[i].y < 0.0f ||
                a[i].y > static_cast<float>(level.height))
                bounded = false;
            if (a[i].x != b[i].x || a[i].y != b[i].y)
                identical = false;
        }
    }
    CHECK(bounded);
    CHECK(identical);
}
