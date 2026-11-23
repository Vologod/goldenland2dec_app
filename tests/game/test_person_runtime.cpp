#include <doctest/doctest.h>

#include "gl/assets/hero_stats.hpp"
#include "gl/game/person.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/script/person_access.hpp"
#include "gl/script/native_registry.hpp"

#include <array>
#include <cstdint>
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

TEST_CASE("person runtime reports parameter presence") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    const auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);
    CHECK(runtime.has_parameter("L1.P1", PersonParameter::strength));
    CHECK_FALSE(runtime.has_parameter("L1.P1", PersonParameter::life));
    CHECK_FALSE(runtime.has_parameter("missing", PersonParameter::strength));
    CHECK_FALSE(runtime.has_parameter("L1.P1", PersonParameter::unknown));
}

TEST_CASE("person runtime keeps the hero entry without save") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    CHECK(runtime.get_parameter("Hero", PersonParameter::reputation) == 0.0);
}

TEST_CASE("person runtime leaves untouched no-save hero parameters absent") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    for (PersonParameter parameter : {PersonParameter::experience, PersonParameter::reputation,
                                      PersonParameter::intelligence, PersonParameter::wisdom,
                                      PersonParameter::strength, PersonParameter::constitution,
                                      PersonParameter::dexterity, PersonParameter::luck,
                                      PersonParameter::perception, PersonParameter::maximum_life,
                                      PersonParameter::life, PersonParameter::energy}) {
        CHECK(runtime.get_parameter("Hero", parameter) == 0.0);
    }

    runtime.set_parameter("Hero", PersonParameter::reputation, 7.0);
    runtime.set_parameter("Hero", PersonParameter::life, 2.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::reputation) == doctest::Approx(7.0));
    CHECK(runtime.get_parameter("Hero", PersonParameter::life) == doctest::Approx(2.0));
    CHECK(runtime.get_parameter("Hero", PersonParameter::strength) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::constitution) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::dexterity) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::perception) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::intelligence) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::wisdom) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::luck) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::experience) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::maximum_life) == 0.0);
    CHECK(runtime.get_parameter("Hero", PersonParameter::energy) == 0.0);
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

TEST_CASE("person runtime reads hero skills from the stat block") {
    gl::assets::HeroStats hero;
    hero.skills[0] = 99;
    hero.skills[1] = -2;
    hero.skills[6] = 10;
    hero.skills[13] = 15;
    auto runtime = gl::game::PersonRuntime::create({}, hero);
    CHECK(runtime.get_skill("Hero", "SKILL_ATHLETIC") == doctest::Approx(10.0));
    CHECK(runtime.get_skill("Hero", "SKILL_WPN_AXE") == doctest::Approx(15.0));
    CHECK(runtime.get_skill("Hero", "SKILL_GODSMAG") == doctest::Approx(15.0));
    CHECK(runtime.get_skill("Hero", "SKILL_ELEMMAG") == doctest::Approx(0.0));
    CHECK(runtime.get_skill("Hero", "SKILL_UNKNOWN") == doctest::Approx(0.0));
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

TEST_CASE("person runtime does not surface primary setter bonuses while the gate is closed") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    const double strength = runtime.get_parameter("L1.P1", PersonParameter::strength);
    const double constitution = runtime.get_parameter("L1.P1", PersonParameter::constitution);
    runtime.set_parameter("L1.P1", PersonParameter::strength, 0.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == strength);
    runtime.set_parameter("L1.P1", PersonParameter::strength, 31.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == strength);
    runtime.set_parameter("L1.P1", PersonParameter::strength, 1.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == strength);
    runtime.set_parameter("L1.P1", PersonParameter::strength, 30.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == strength);
    runtime.set_parameter("L1.P1", PersonParameter::constitution, 12.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::constitution) == constitution);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == strength);
}

TEST_CASE("person runtime writes the Server.dll strength bonus without exposing it") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(10.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::constitution) == doctest::Approx(8.0));

    runtime.set_parameter("L1.P1", PersonParameter::constitution, 12.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::constitution) == doctest::Approx(8.0));
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(10.0));

    runtime.set_parameter("L1.P1", PersonParameter::strength, 14.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(10.0));

    runtime.set_parameter("L1.P1", PersonParameter::strength, 30.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::strength) == doctest::Approx(10.0));
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

TEST_CASE("person runtime clamps positive energy to the computed maximum") {
    auto runtime = gl::game::PersonRuntime::create({make_person("L1.P1", "L1.P1")}, std::nullopt);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::energy) == doctest::Approx(0.0));
    runtime.set_parameter("L1.P1", PersonParameter::energy, 100.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::energy) == doctest::Approx(18.0));
    runtime.set_parameter("L1.P1", PersonParameter::energy, -5.0);
    CHECK(runtime.get_parameter("L1.P1", PersonParameter::energy) == doctest::Approx(0.0));
}

TEST_CASE("hero level from experience matches FUN_1400cbc0") {
    CHECK(gl::game::hero_level_from_experience(0.0) == 1);
    CHECK(gl::game::hero_level_from_experience(99.0) == 1);
    CHECK(gl::game::hero_level_from_experience(100.0) == 2);
    CHECK(gl::game::hero_level_from_experience(9485.0) == 14);
    CHECK(gl::game::hero_level_from_experience(65133.0) == 36);
    CHECK(gl::game::hero_level_from_experience(81099.0) == 40);
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

TEST_CASE("person runtime stores the undead flag") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);
    CHECK_FALSE(runtime.undead("L1.P1"));
    runtime.set_undead("L1.P1", true);
    CHECK(runtime.undead("L1.P1"));
    runtime.set_undead("missing", true);
    CHECK_FALSE(runtime.undead("missing"));
    runtime.set_undead("L1.P1", false);
    CHECK_FALSE(runtime.undead("L1.P1"));
}

TEST_CASE("set-undead native dispatches string names") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);
    gl::script::NativeServices services;
    services.person = runtime.access();
    const std::vector<gl::script::Value> args{std::string{"L1.P1"}, double{1.0}};
    const auto *info = gl::script::NativeRegistry::find(0x700000f);
    REQUIRE(info != nullptr);
    CHECK(gl::script::NativeRegistry::call(*info, args, services) ==
          gl::script::Value{double{0.0}});
    CHECK(runtime.undead("L1.P1"));
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 2);
}

TEST_CASE("person party role updates known persons only") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    CHECK(runtime.party_role("Hero") == 0);
    runtime.set_party_role("Hero", 1);
    CHECK(runtime.party_role("Hero") == 1);
    runtime.set_party_role("missing", 2);
    CHECK(runtime.party_role("missing") == 0);
}

TEST_CASE("injury records use the fixed SetInjured key") {
    const std::array<std::int32_t, 5> key{22, -49, 129, 0, -1};
    std::vector<gl::game::PersonEffect> records{{1, 2, 3, 4, 5}};

    gl::game::set_injured_records(records, true);
    REQUIRE(records.size() == 2);
    CHECK(records.back().words == key);

    gl::game::set_injured_records(records, true);
    REQUIRE(records.size() == 2);
    CHECK(records.back().words == key);

    gl::game::set_injured_records(records, false);
    REQUIRE(records.size() == 1);
    CHECK(records.front().words == std::array<std::int32_t, 5>{1, 2, 3, 4, 5});
}

TEST_CASE("person runtime set-injured toggles known persons only") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);

    CHECK_FALSE(runtime.injured("L1.P1"));
    runtime.set_injured("L1.P1", true);
    CHECK(runtime.injured("L1.P1"));
    runtime.set_injured("missing", true);
    CHECK_FALSE(runtime.injured("missing"));
    runtime.set_injured("L1.P1", false);
    CHECK_FALSE(runtime.injured("L1.P1"));
}

TEST_CASE("dialog percent reproduces the talking formula") {
    const auto hero = [](std::int32_t intelligence, std::int32_t talking,
                         std::int32_t trade) {
        gl::assets::HeroStats stats;
        stats.intelligence = intelligence;
        stats.skills[21] = talking;
        stats.skills[16] = trade;
        return stats;
    };

    auto silent = gl::game::PersonRuntime::create({}, hero(8, 0, 10));
    CHECK(silent.dialog_percent("Hero") == 0);

    auto maxed = gl::game::PersonRuntime::create({}, hero(0, 15, 0));
    CHECK(maxed.dialog_percent("Hero") == 100);

    auto base = gl::game::PersonRuntime::create({}, hero(7, 5, 0));
    CHECK(base.dialog_percent("Hero") == 37);

    auto trader = gl::game::PersonRuntime::create({}, hero(7, 5, 10));
    CHECK(trader.dialog_percent("Hero") == 57);

    auto talking_bonus = gl::game::PersonRuntime::create({}, hero(5, 10, 9));
    CHECK(talking_bonus.dialog_percent("Hero") == 57);

    auto negative = gl::game::PersonRuntime::create({}, hero(7, -3, 10));
    CHECK(negative.dialog_percent("Hero") == 0);

    auto low_int = gl::game::PersonRuntime::create({}, hero(0, 5, 0));
    CHECK(low_int.dialog_percent("Hero") == 31);

    auto high_int = gl::game::PersonRuntime::create({}, hero(99, 5, 0));
    CHECK(high_int.dialog_percent("Hero") == 60);

    CHECK(base.dialog_percent("missing") == 0);
}

TEST_CASE("dialog-enabled native gates the percent with the shared coin") {
    gl::assets::HeroStats stats;
    stats.intelligence = 7;
    stats.skills[21] = 5;
    auto runtime = gl::game::PersonRuntime::create({}, stats);

    gl::script::NativeServices services;
    services.person = runtime.access();
    bool gate = true;
    double probability = -1.0;
    services.random.coin = [&](double value) {
        probability = value;
        return gate;
    };

    const auto *info = gl::script::NativeRegistry::find(0x700000e);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 1);

    const std::vector<gl::script::Value> args{double{2.0}};
    CHECK(gl::script::NativeRegistry::call(*info, args, services) ==
          gl::script::Value{double{1.0}});
    CHECK(probability == doctest::Approx(0.37));

    gate = false;
    CHECK(gl::script::NativeRegistry::call(*info, args, services) ==
          gl::script::Value{double{0.0}});
}

TEST_CASE("dialog-enabled native rejects invalid arguments and missing services") {
    const auto *info = gl::script::NativeRegistry::find(0x700000e);
    REQUIRE(info != nullptr);
    gl::script::NativeServices services;

    const std::vector<gl::script::Value> string_arg{std::string{"Hero"}};
    CHECK(gl::script::NativeRegistry::call(*info, string_arg, services) ==
          gl::script::Value{double{0.0}});

    const std::vector<gl::script::Value> empty;
    CHECK(gl::script::NativeRegistry::call(*info, empty, services) ==
          gl::script::Value{std::int32_t{0}});

    const std::vector<gl::script::Value> args{double{2.0}};
    CHECK(gl::script::NativeRegistry::call(*info, args, services) ==
          gl::script::Value{double{0.0}});
}

TEST_CASE("set-injured native forwards the person and flag") {
    const std::vector<gl::game::PersonInstance> persons{make_person("L1.P1", "L1.P1")};
    auto runtime = gl::game::PersonRuntime::create(persons, std::nullopt);
    gl::script::NativeServices services;
    services.person = runtime.access();

    const auto *info = gl::script::NativeRegistry::find(0x7000013);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 2);

    const std::vector<gl::script::Value> on{std::string{"L1.P1"}, double{1.0}};
    const std::vector<gl::script::Value> off{std::string{"L1.P1"}, double{0.0}};
    CHECK(gl::script::NativeRegistry::call(*info, on, services) ==
          gl::script::Value{double{0.0}});
    CHECK(runtime.injured("L1.P1"));
    CHECK(gl::script::NativeRegistry::call(*info, off, services) ==
          gl::script::Value{double{0.0}});
    CHECK_FALSE(runtime.injured("L1.P1"));
}

TEST_CASE("person access exposes dialog percent and set-injured") {
    gl::assets::HeroStats stats;
    stats.intelligence = 7;
    stats.skills[21] = 5;
    auto runtime = gl::game::PersonRuntime::create({}, stats);
    auto access = runtime.access();
    REQUIRE(access.dialog_percent);
    CHECK(access.dialog_percent() == 37);
    REQUIRE(access.set_injured);
    access.set_injured("Hero", true);
    CHECK(runtime.injured("Hero"));
}
