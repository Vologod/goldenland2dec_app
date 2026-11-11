#include "gl/game/person_rng.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>

using gl::assets::PersonDef;
using gl::assets::PersonWeapon;
using gl::game::consume_person_init_rng;
using gl::game::MsvcRandom;

namespace {
PersonWeapon make_weapon(std::string name, std::int32_t min_level, std::int32_t max_level) {
    return PersonWeapon{std::move(name), min_level, max_level};
}
} // namespace

TEST_CASE("person rng: no weapon block consumes only the spawn draw") {
    PersonDef person;
    person.level = 1;
    MsvcRandom rng(1);
    const auto rolls = consume_person_init_rng(person, rng);
    CHECK_FALSE(rolls.weapon_index.has_value());
    CHECK(rolls.spawn_roll == 3008);
    CHECK(rng.next() == 18467u);
}

TEST_CASE("person rng: first coin success selects the first eligible weapon") {
    PersonDef person;
    person.level = 1;
    person.has_weapon = true;
    person.weapons = {make_weapon("A", 1, 10), make_weapon("B", -100, 10)};

    MsvcRandom rng(1);
    const auto rolls = consume_person_init_rng(person, rng);
    REQUIRE(rolls.weapon_index.has_value());
    CHECK(*rolls.weapon_index == 0u);
    CHECK(rolls.spawn_roll >= 3000);
    CHECK(rolls.spawn_roll <= 10000);
    CHECK(rng.next() == 6334u);
}

TEST_CASE("person rng: failed coin advances to the next eligible weapon") {
    PersonDef person;
    person.level = 20;
    person.weapon_level_offset = -7;
    person.has_weapon = true;
    person.weapons = {make_weapon("HIGH", 18, 200), make_weapon("MID", 12, 22),
                      make_weapon("LOW", -100, 10)};

    MsvcRandom rng(1);
    CHECK(rng.next() == 41u);
    const auto rolls = consume_person_init_rng(person, rng);
    REQUIRE(rolls.weapon_index.has_value());
    CHECK(*rolls.weapon_index == 0u);
    CHECK(rng.next() == 19169u);
}

TEST_CASE("person rng: wrap returns to index zero with one eligible weapon") {
    PersonDef person;
    person.level = 1;
    person.has_weapon = true;
    person.weapons = {make_weapon("ONLY", 1, 10)};

    MsvcRandom rng(1);
    CHECK(rng.next() == 41u);
    const auto rolls = consume_person_init_rng(person, rng);
    REQUIRE(rolls.weapon_index.has_value());
    CHECK(*rolls.weapon_index == 0u);
    CHECK(rng.next() == 19169u);
}

TEST_CASE("person rng: two eligible weapons can select the second index") {
    PersonDef person;
    person.level = 1;
    person.has_weapon = true;
    person.weapons = {make_weapon("A", 1, 10), make_weapon("B", 1, 10)};

    MsvcRandom rng(1);
    CHECK(rng.next() == 41u);
    const auto rolls = consume_person_init_rng(person, rng);
    REQUIRE(rolls.weapon_index.has_value());
    CHECK(*rolls.weapon_index == 1u);
    CHECK(rng.next() == 19169u);
}

TEST_CASE("person rng: weapon block without eligible rows consumes no weapon draw") {
    PersonDef person;
    person.level = 500;
    person.has_weapon = true;
    person.weapons = {make_weapon("LOW", 1, 10)};

    MsvcRandom rng(1);
    const auto rolls = consume_person_init_rng(person, rng);
    CHECK_FALSE(rolls.weapon_index.has_value());
    CHECK(rolls.spawn_roll == 3008);
    CHECK(rng.next() == 18467u);
}
