#include <doctest/doctest.h>

#include "gl/assets/hero_stats.hpp"
#include "gl/game/person.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/script/person_access.hpp"

#include <string>
#include <utility>
#include <vector>

namespace {
gl::game::PersonInstance make_person(std::string name, std::string res) {
    gl::game::PersonInstance person;
    person.name = std::move(name);
    person.res_name = res;
    person.def.res_name = res;
    person.def.values["strength"] = "10";
    person.def.values["constitution"] = "8";
    person.def.values["dexterity"] = "7";
    person.def.values["perception"] = "6";
    person.def.values["intelligence"] = "5";
    person.def.values["wisdom"] = "4";
    person.def.values["luck"] = "3";
    person.def.values["reputation"] = "2";
    person.def.values["experience_value"] = "0";
    person.def.values["skills.SKILL_WPN_AXE"] = "9";
    return person;
}

using gl::script::PersonParameter;
} // namespace

TEST_CASE("person runtime finds NPCs by exact name") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) != 0.0);
    CHECK(runtime.get_parameter("l1.p1", PersonParameter::strength) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::strength) == 0.0);
}

TEST_CASE("person runtime keeps the hero entry without save") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    CHECK(runtime.get_parameter("Hero", PersonParameter::reputation) == 0.0);
}

TEST_CASE("person runtime applies save hero stats") {
    gl::assets::HeroStats hero;
    hero.reputation = 11;
    hero.experience = 1234;
    hero.life = 263;
    auto runtime = gl::game::PersonRuntime::create({}, hero);
    CHECK(runtime.get_parameter("Hero", PersonParameter::reputation) != 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::experience) == doctest::Approx(1234.0));
}

TEST_CASE("person runtime set/get roundtrip") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    runtime.set_parameter("L1.P1", PersonParameter::life, 7.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::life) == doctest::Approx(7.0));
}

TEST_CASE("person runtime reads skills") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    CHECK(runtime.get_skill("L1.P1", "SKILL_WPN_AXE") == doctest::Approx(9.0));
    CHECK(runtime.get_skill("L1.P1", "SKILL_UNKNOWN") == doctest::Approx(0.0));
}

TEST_CASE("person runtime exposes access callbacks") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    auto access = runtime.access();
    REQUIRE(access.get_parameter);
    REQUIRE(access.set_parameter);
    REQUIRE(access.get_skill);
    CHECK(access.get_parameter("L1.P1", PersonParameter::strength) > 0.0);
    access.set_parameter("L1.P1", PersonParameter::life, 12.0);
    CHECK(access.get_parameter("L1.P1", PersonParameter::life) == doctest::Approx(12.0));
    CHECK(access.get_skill("L1.P1", "SKILL_WPN_AXE") == doctest::Approx(9.0));
}

TEST_CASE("person runtime clamps primary stats to the configured range") {
    auto person = make_person("L1.P1", "L1.P1");
    person.def.values["strength"] = "0";
    person.def.values["constitution"] = "99";
    person.def.values["luck"] = "-5";
    auto runtime = gl::game::PersonRuntime::create({person}, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(1.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::constitution) == doctest::Approx(30.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::luck) == doctest::Approx(1.0));
}

TEST_CASE("person runtime setter rejects values outside the primary range") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    const double strength = runtime.get_parameter("L1.P1", PersonParameter::strength);
    runtime.set_parameter("L1.P1", PersonParameter::strength, 0.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == strength);
    runtime.set_parameter("L1.P1", PersonParameter::strength, 31.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == strength);
    runtime.set_parameter("L1.P1", PersonParameter::strength, 1.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(1.0));
    runtime.set_parameter("L1.P1", PersonParameter::strength, 30.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(30.0));
}

TEST_CASE("person runtime reproduces the Server.dll strength bonus quirk") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(10.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::constitution) == doctest::Approx(8.0));

    runtime.set_parameter("L1.P1", PersonParameter::constitution, 12.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::constitution) == doctest::Approx(8.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(14.0));

    runtime.set_parameter("L1.P1", PersonParameter::strength, 14.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(14.0));

    runtime.set_parameter("L1.P1", PersonParameter::strength, 30.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(30.0));
}

TEST_CASE("person runtime clamps reputation and rounds half away from zero") {
    auto runtime = gl::game::PersonRuntime::create(
        {make_person("HIGH", "HIGH"), make_person("LOW", "LOW"), make_person("FRAC", "FRAC")},
        std::nullopt);
    auto high = make_person("HIGH", "HIGH");
    high.def.values["reputation"] = "100";
    auto low = make_person("LOW", "LOW");
    low.def.values["reputation"] = "-100";
    auto frac = make_person("FRAC", "FRAC");
    frac.def.values["reputation"] = "2.5";
    const std::vector<gl::game::PersonInstance> persons{high, low, frac};
    auto clamped = gl::game::PersonRuntime::create(persons, std::nullopt);
    CHECK(clamped.get_parameter("HIGH", PersonParameter::reputation) == doctest::Approx(50.0));
    CHECK(clamped.get_parameter("LOW", PersonParameter::reputation) == doctest::Approx(-50.0));
    CHECK(clamped.get_parameter("FRAC", PersonParameter::reputation) == doctest::Approx(3.0));

    runtime.set_parameter("HIGH", PersonParameter::reputation, 100.0);
    CHECK(runtime.get_parameter("HIGH", PersonParameter::reputation) == doctest::Approx(50.0));
}

TEST_CASE("person runtime reputation setter applies magic skill multipliers") {
    auto light = make_person("LIGHT", "LIGHT");
    light.def.values["reputation"] = "0";
    light.def.values["skills.SKILL_LGHTMAG"] = "5";
    auto dark = make_person("DARK", "DARK");
    dark.def.values["reputation"] = "0";
    dark.def.values["skills.SKILL_DARKMAG"] = "5";
    auto runtime = gl::game::PersonRuntime::create({light, dark}, std::nullopt);

    runtime.set_parameter("LIGHT", PersonParameter::reputation, 10.0);
    CHECK(runtime.get_parameter("LIGHT", PersonParameter::reputation) == doctest::Approx(20.0));

    runtime.set_parameter("DARK", PersonParameter::reputation, -10.0);
    CHECK(runtime.get_parameter("DARK", PersonParameter::reputation) == doctest::Approx(-20.0));
}

TEST_CASE("person runtime computes maximum life and energy") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::maximum_life) == doctest::Approx(40.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::energy) == doctest::Approx(0.0));
}

TEST_CASE("person runtime converts experience to level for maximum life and energy") {
    auto person = make_person("L1.P1", "L1.P1");
    person.def.values["strength"] = "30";
    person.def.values["constitution"] = "25";
    person.def.values["intelligence"] = "5";
    person.def.values["wisdom"] = "7";
    person.def.values["experience_value"] = "81099";
    auto runtime = gl::game::PersonRuntime::create({person}, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::experience) == doctest::Approx(81099.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::maximum_life) == doctest::Approx(216.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::maximum_life) != doctest::Approx(263.0));
}

TEST_CASE("person runtime clamps experience and life") {
    auto person = make_person("L1.P1", "L1.P1");
    person.def.values["experience_value"] = "600000";
    auto runtime = gl::game::PersonRuntime::create({person}, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::experience) == doctest::Approx(500000.0));

    runtime.set_parameter("L1.P1", PersonParameter::life, -5.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::life) == doctest::Approx(0.0));
    runtime.set_parameter("L1.P1", PersonParameter::life, 1000.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::life) ==
          doctest::Approx(runtime.get_parameter("L1.P1", PersonParameter::maximum_life)));
}

TEST_CASE("person runtime applies SKILL_TALKING bonuses to intelligence and wisdom") {
    auto none = make_person("NONE", "NONE");
    auto five = make_person("FIVE", "FIVE");
    five.def.values["skills.SKILL_TALKING"] = "5";
    auto ten = make_person("TEN", "TEN");
    ten.def.values["skills.SKILL_TALKING"] = "10";
    auto capped = make_person("CAPPED", "CAPPED");
    capped.def.values["intelligence"] = "29";
    capped.def.values["skills.SKILL_TALKING"] = "10";
    auto runtime = gl::game::PersonRuntime::create({none, five, ten, capped}, std::nullopt);

    CHECK(runtime.get_parameter("NONE", PersonParameter::intelligence) == doctest::Approx(5.0));
    CHECK(runtime.get_parameter("NONE", PersonParameter::wisdom) == doctest::Approx(4.0));
    CHECK(runtime.get_parameter("FIVE", PersonParameter::intelligence) == doctest::Approx(5.0));
    CHECK(runtime.get_parameter("FIVE", PersonParameter::wisdom) == doctest::Approx(5.0));
    CHECK(runtime.get_parameter("TEN", PersonParameter::intelligence) == doctest::Approx(7.0));
    CHECK(runtime.get_parameter("TEN", PersonParameter::wisdom) == doctest::Approx(5.0));
    CHECK(runtime.get_parameter("CAPPED", PersonParameter::intelligence) == doctest::Approx(30.0));
}

TEST_CASE("person runtime ignores maximum life and experience setters") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    const double maximum = runtime.get_parameter("L1.P1", PersonParameter::maximum_life);
    runtime.set_parameter("L1.P1", PersonParameter::maximum_life, 999.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::maximum_life) == maximum);
    runtime.set_parameter("L1.P1", PersonParameter::experience, 123.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::experience) == doctest::Approx(0.0));
}

TEST_CASE("person runtime unknown parameters are zero and setters are no-ops") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::unknown) == 0.0);
    runtime.set_parameter("L1.P1", PersonParameter::unknown, 5.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(10.0));
}

TEST_CASE("person runtime handles missing persons as defaults") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    CHECK(runtime.get_parameter("Missing", PersonParameter::strength) == 0.0);
    runtime.set_parameter("Missing", PersonParameter::strength, 20.0);
    CHECK(runtime.get_parameter("Missing", PersonParameter::strength) == 0.0);
    CHECK(runtime.get_skill("Missing", "SKILL_WPN_AXE") == 0.0);
}

TEST_CASE("person runtime keeps the first duplicate person name") {
    auto first = make_person("DUP", "DUP");
    first.def.values["strength"] = "11";
    auto second = make_person("DUP", "DUP");
    second.def.values["strength"] = "22";
    auto runtime = gl::game::PersonRuntime::create({first, second}, std::nullopt);
    CHECK(runtime.get_parameter("DUP", PersonParameter::strength) == doctest::Approx(11.0));
}
