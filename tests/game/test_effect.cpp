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
    CHECK(gl::game::effect_delay_pending(make_effect(9, -3, 0x20, 0x0000000A)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x20, 0x23000000)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x60, 0x23000000)));
    CHECK_FALSE(gl::game::effect_delay_pending(make_effect(9, -3, 0x40, 0)));
}

TEST_CASE("age_effects decrements 0x10 duration and removes on expiry") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x10, 0x00000064)};
    gl::game::age_effects(effects, 40);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x0000003C);
    gl::game::age_effects(effects, 60);
    CHECK(effects.empty());
}

TEST_CASE("age_effects handles 0x20 delay then active duration") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x20, 0x0023000A)};
    gl::game::age_effects(effects, 5);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x00230005);
    gl::game::age_effects(effects, 5);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x00230000);
    gl::game::age_effects(effects, 35);
    CHECK(effects.empty());
}

TEST_CASE("age_effects overwrites the whole cond in the 0x10 branch") {
    std::vector<gl::game::PersonEffect> effects{
        make_effect(9, -3, 0x10, 0xAB000064)};
    gl::game::age_effects(effects, 40);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x0000003C);
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
        make_effect(7, 2, 0x20, 0x00000005),
    };
    const auto mods = gl::game::active_effect_mods(effects);
    REQUIRE(mods.size() == 1);
    CHECK(mods[0].id == 9);
    CHECK(mods[0].value == -3);
    CHECK(mods[0].flags == 0x60);
    CHECK(mods[0].word3 == 0x23000000);
}
