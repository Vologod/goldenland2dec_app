#include <doctest/doctest.h>

#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/person.hpp"
#include "gl/game/person_runtime.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

gl::game::PersonRuntime make_slot_runtime(int slot) {
    auto body = gl::assets::decode_save_body(read_save(slot));
    REQUIRE(body.has_value());
    auto stats = gl::assets::parse_hero_stats(body.value());
    REQUIRE(stats.has_value());
    return gl::game::PersonRuntime::create({}, stats.value());
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
