#include <doctest/doctest.h>

#include "gl/game/effect.hpp"

#include <cstdint>
#include <vector>

namespace {
gl::game::PersonEffect make_effect(std::int32_t id, std::int32_t value, std::uint32_t flags,
                                   std::uint32_t cond, std::int32_t extra = -1) {
    gl::game::PersonEffect effect;
    effect.words = {id, value, static_cast<std::int32_t>(flags), static_cast<std::int32_t>(cond),
                    extra};
    return effect;
}
} // namespace

TEST_CASE("effect_delay_pending follows the client skip rule") {
    CHECK(gl::game::effect_delay_pending(make_effect(9, -3, 0x20, 0x00000A00)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x20, 0x23000000)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x60, 0x23000000)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x40, 0)));
}

TEST_CASE("age_effects decrements 0x10 duration and removes on expiry") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x10, 0x00002801)};
    gl::game::age_effects(effects, 40);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x00000001);
    gl::game::age_effects(effects, 60);
    CHECK(effects.empty());
}

TEST_CASE("age_effects handles 0x20 delay then active duration") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x20, 0x23000A00)};
    gl::game::age_effects(effects, 5);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x23000500);
    gl::game::age_effects(effects, 5);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x23000000);
    gl::game::age_effects(effects, 35);
    CHECK(effects.empty());
}

TEST_CASE("age_effects overwrites the whole cond in the 0x10 branch") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x10, 0xAB002801)};
    gl::game::age_effects(effects, 40);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x00000001);
}

TEST_CASE("age_effects is a no-op for zero elapsed time") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x60, 0x23000000)};
    gl::game::age_effects(effects, 0);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x23000000);
}

TEST_CASE("total_minutes and minutes_between use whole minutes") {
    gl::game::GameDatetime a;
    a.year = 1124;
    a.month = 11;
    a.day = 27;
    a.hour = 4;
    a.minute = 0;
    gl::game::GameDatetime b = a;
    b.minute = 35;
    b.second = 59;
    CHECK(gl::game::minutes_between(a, b) == 35);
    CHECK(gl::game::minutes_between(b, a) == 0);
    CHECK(gl::game::minutes_between(gl::game::GameDatetime{}, b) == 0);
}

TEST_CASE("active_effect_mods drops pending delay records") {
    const std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x60, 0x23000000),
        make_effect(7, 2, 0x20, 0x00000500),
    };
    const auto mods = gl::game::active_effect_mods(effects);
    REQUIRE(mods.size() == 1);
    CHECK(mods[0].id == 9);
    CHECK(mods[0].value == -3);
    CHECK(mods[0].flags == 0x60);
    CHECK(mods[0].word3 == 0x23000000);
}

#include "gl/game/person.hpp"
#include "gl/game/person_runtime.hpp"

TEST_CASE("PersonRuntime restores saved effects and exposes active mods") {
    gl::game::PersonInstance person;
    person.name = "A";
    person.route_type = "STAY";
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x60, 0x23000000, -1};
    person.saved_effects.push_back(effect);
    auto runtime = gl::game::PersonRuntime::create({person}, std::nullopt);

    REQUIRE(runtime.effects("A").size() == 1);
    const auto mods = runtime.active_effect_mods("A");
    REQUIRE(mods.size() == 1);
    CHECK(mods[0].id == 9);
    CHECK(mods[0].value == -3);
    CHECK(runtime.effects("missing").empty());
    CHECK(runtime.active_effect_mods("missing").empty());
}

TEST_CASE("PersonRuntime ages each person from its own clock") {
    gl::game::PersonInstance person;
    person.name = "A";
    person.route_type = "STAY";
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x20, 0x00000A00, -1};
    person.saved_effects.push_back(effect);
    gl::game::GameDatetime start;
    start.year = 1124;
    start.month = 11;
    start.day = 27;
    start.hour = 4;
    start.minute = 0;
    person.effect_clock = start;
    auto runtime = gl::game::PersonRuntime::create({person}, std::nullopt);

    gl::game::GameDatetime now = start;
    now.minute = 5;
    runtime.age_effects(now);
    REQUIRE(runtime.effects("A").size() == 1);
    CHECK(static_cast<std::uint32_t>(runtime.effects("A")[0].words[3]) == 0x00000500);

    now.minute = 10;
    runtime.age_effects(now);
    now.minute = 20;
    runtime.age_effects(now);
    CHECK(runtime.active_effect_mods("A").empty());
}

TEST_CASE("PersonRuntime set_effects and set_effect_clock replace state") {
    auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    gl::game::PersonEffect effect;
    effect.words = {9, -3, 0x40, 0, -1};
    runtime.set_effects("Hero", {effect});
    REQUIRE(runtime.effects("Hero").size() == 1);
    gl::game::GameDatetime clock;
    clock.year = 1124;
    runtime.set_effect_clock("Hero", clock);
    REQUIRE(runtime.effect_clock("Hero").has_value());
    CHECK(runtime.effect_clock("Hero")->year == 1124);
}

#include "gl/game/combat_stats.hpp"

TEST_CASE("compute_armor_weight applies effect mods") {
    gl::game::StatInputs inputs;
    inputs.strength = 10;
    inputs.constitution = 15;
    inputs.dexterity = 10;
    inputs.perception = 21;
    inputs.luck = 10;
    inputs.wisdom = 5;
    inputs.intelligence = 11;
    inputs.experience = 14155;
    inputs.skills.fill(0);

    const auto without = gl::game::compute_armor_weight(inputs);
    gl::assets::SpecialMod effect;
    effect.id = 9;
    effect.value = -3;
    effect.flags = 0x60;
    effect.word3 = 0x23000000;
    inputs.effect_mods.push_back(effect);
    const auto with = gl::game::compute_armor_weight(inputs);

    CHECK(without.primary[2] == 10);
    CHECK(with.primary[2] == 7);
}
