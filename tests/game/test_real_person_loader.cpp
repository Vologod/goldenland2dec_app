#include "gl/assets/pad.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/person.hpp"

#include <cstdlib>
#include <doctest/doctest.h>
#include <string>

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
