#include <doctest/doctest.h>

#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/person.hpp"
#include "gl/game/person_runtime.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

gl::assets::HeroStats load_stats(int slot) {
    auto body = gl::assets::decode_save_body(read_save(slot));
    REQUIRE(body.has_value());
    auto stats = gl::assets::parse_hero_stats(body.value());
    REQUIRE(stats.has_value());
    return stats.value();
}

gl::game::PersonRuntime make_slot_runtime(int slot) {
    return gl::game::PersonRuntime::create({}, load_stats(slot));
}

void check_hero(gl::game::PersonRuntime &runtime, double strength, double constitution,
                double dexterity, double perception, double intelligence, double wisdom,
                double luck, double reputation, double experience, double maximum_life, double life,
                double energy) {
    using gl::script::PersonParameter;
    CHECK(runtime.get_parameter("Hero", PersonParameter::strength) == doctest::Approx(strength));
    CHECK(runtime.get_parameter("Hero", PersonParameter::constitution) ==
          doctest::Approx(constitution));
    CHECK(runtime.get_parameter("Hero", PersonParameter::dexterity) == doctest::Approx(dexterity));
    CHECK(runtime.get_parameter("Hero", PersonParameter::perception) ==
          doctest::Approx(perception));
    CHECK(runtime.get_parameter("Hero", PersonParameter::intelligence) ==
          doctest::Approx(intelligence));
    CHECK(runtime.get_parameter("Hero", PersonParameter::wisdom) == doctest::Approx(wisdom));
    CHECK(runtime.get_parameter("Hero", PersonParameter::luck) == doctest::Approx(luck));
    CHECK(runtime.get_parameter("Hero", PersonParameter::reputation) ==
          doctest::Approx(reputation));
    CHECK(runtime.get_parameter("Hero", PersonParameter::experience) ==
          doctest::Approx(experience));
    CHECK(runtime.get_parameter("Hero", PersonParameter::maximum_life) ==
          doctest::Approx(maximum_life));
    CHECK(runtime.get_parameter("Hero", PersonParameter::life) == doctest::Approx(life));
    CHECK(runtime.get_parameter("Hero", PersonParameter::energy) == doctest::Approx(energy));
}
} // namespace

TEST_CASE("real data: l1_1 NPC effective person parameters" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto persons = gl::game::load_persons(vfs.value(), "l1_1");
    REQUIRE(persons.has_value());
    REQUIRE(!persons->empty());

    auto runtime = gl::game::PersonRuntime::create(persons.value(), std::nullopt);
    using gl::script::PersonParameter;
    const std::string name = "L1_1.P387_Poison_Plotnik";
    CHECK(runtime.get_parameter(name, PersonParameter::strength) == doctest::Approx(14.0));
    CHECK(runtime.get_parameter(name, PersonParameter::constitution) == doctest::Approx(14.0));
    CHECK(runtime.get_parameter(name, PersonParameter::dexterity) == doctest::Approx(18.0));
    CHECK(runtime.get_parameter(name, PersonParameter::perception) == doctest::Approx(18.0));
    CHECK(runtime.get_parameter(name, PersonParameter::intelligence) == doctest::Approx(9.0));
    CHECK(runtime.get_parameter(name, PersonParameter::wisdom) == doctest::Approx(9.0));
    CHECK(runtime.get_parameter(name, PersonParameter::luck) == doctest::Approx(11.0));
    CHECK(runtime.get_parameter(name, PersonParameter::reputation) == doctest::Approx(0.0));
    CHECK(runtime.get_parameter(name, PersonParameter::experience) == doctest::Approx(20.0));
    CHECK(runtime.get_parameter(name, PersonParameter::maximum_life) == doctest::Approx(64.0));
    CHECK(runtime.get_parameter(name, PersonParameter::life) == doctest::Approx(0.0));
    CHECK(runtime.get_parameter(name, PersonParameter::energy) == doctest::Approx(0.0));
    CHECK(runtime.get_skill(name, "SKILL_HEALING") == doctest::Approx(15.0));
    CHECK(runtime.get_skill(name, "SKILL_TACTIC") == doctest::Approx(15.0));
    CHECK(runtime.get_skill(name, "SKILL_MAGICUSE") == doctest::Approx(4.0));
}

TEST_CASE("real data: slot0 hero effective person parameters" *
          doctest::skip(!game_dir_available())) {
    auto runtime = make_slot_runtime(0);
    check_hero(runtime, 30.0, 25.0, 18.0, 12.0, 5.0, 7.0, 9.0, -40.0, 81099.0, 216.0, 216.0, 0.0);
}

TEST_CASE("real data: slot2 hero effective person parameters" *
          doctest::skip(!game_dir_available())) {
    auto runtime = make_slot_runtime(2);
    check_hero(runtime, 24.0, 21.0, 22.0, 10.0, 8.0, 8.0, 10.0, -50.0, 65133.0, 170.0, 170.0, 0.0);
}

TEST_CASE("real data: raw over-maximum life is clamped on get" *
          doctest::skip(!game_dir_available())) {
    struct Expected {
        int slot;
        std::int32_t raw_life;
        double maximum_life;
    };
    const Expected slots[] = {{0, 263, 216.0}, {2, 201, 170.0}};

    for (const auto &expected : slots) {
        const gl::assets::HeroStats stats = load_stats(expected.slot);
        CHECK(stats.life == expected.raw_life);
        auto runtime = gl::game::PersonRuntime::create({}, stats);
        CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::maximum_life) ==
              doctest::Approx(expected.maximum_life));
        CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::life) ==
              doctest::Approx(expected.maximum_life));
    }
}

TEST_CASE("real data: hero skills come from the save stat block" *
          doctest::skip(!game_dir_available())) {
    struct Expected {
        int slot;
        std::string_view skill;
        double value;
    };
    const Expected slots[] = {
        {0, "SKILL_ATHLETIC", 10.0}, {0, "SKILL_WPN_AXE", 15.0}, {0, "SKILL_SCOUT", 0.0},
        {0, "SKILL_TRADE", 1.0},     {0, "SKILL_HEALING", 14.0}, {0, "SKILL_CRITICAL_HIT", 15.0},
        {2, "SKILL_ATHLETIC", 13.0}, {2, "SKILL_WPN_AXE", 15.0}, {2, "SKILL_SCOUT", 1.0},
        {2, "SKILL_TRADE", 5.0},     {2, "SKILL_HEALING", 1.0},  {2, "SKILL_CRITICAL_HIT", 15.0},
    };

    for (const auto &expected : slots) {
        auto runtime = make_slot_runtime(expected.slot);
        CHECK(runtime.get_skill("Hero", expected.skill) == doctest::Approx(expected.value));
    }
}
