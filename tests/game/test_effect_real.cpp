#include <doctest/doctest.h>

#include "gl/assets/hero_save_body.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/combat_stats.hpp"
#include "gl/game/effect.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(const std::string &name) {
    const std::filesystem::path path =
        std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" / name;
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}

std::vector<std::uint8_t> hero_body(int slot) {
    auto body = gl::assets::decode_save_body(read_save("slot" + std::to_string(slot) + ".sav"));
    REQUIRE(body.has_value());
    return body.value();
}

gl::game::PersonEffect effect_at(const std::array<std::uint8_t, 0x14> &raw) {
    gl::game::PersonEffect effect;
    for (std::size_t i = 0; i < effect.words.size(); ++i) {
        effect.words[i] = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(raw[4 * i]) |
            (static_cast<std::uint32_t>(raw[4 * i + 1]) << 8) |
            (static_cast<std::uint32_t>(raw[4 * i + 2]) << 16) |
            (static_cast<std::uint32_t>(raw[4 * i + 3]) << 24));
    }
    return effect;
}
} // namespace

TEST_CASE("real saves: hero effects are 0 except slot16" *
          doctest::skip(!game_dir_available())) {
    for (int slot = 0; slot <= 27; ++slot) {
        const auto body = gl::assets::parse_hero_save_body(hero_body(slot));
        REQUIRE(body.has_value());
        if (slot == 16) {
            REQUIRE(body->extra.size() == 1);
            const auto effect = effect_at(body->extra[0]);
            CHECK(effect.words[0] == 9);
            CHECK(effect.words[1] == -3);
            CHECK(static_cast<std::uint32_t>(effect.words[2]) == 0x60);
            CHECK(static_cast<std::uint32_t>(effect.words[3]) == 0x23000000);
            CHECK(effect.words[4] == -1);
        } else {
            CHECK(body->extra.empty());
        }
    }
}

TEST_CASE("real slot16: effect ages out after 35 game minutes" *
          doctest::skip(!game_dir_available())) {
    const auto body = gl::assets::parse_hero_save_body(hero_body(16));
    REQUIRE(body.has_value());
    REQUIRE(body->extra.size() == 1);
    std::vector<gl::game::PersonEffect> effects{effect_at(body->extra[0])};
    gl::game::age_effects(effects, 10);
    REQUIRE(effects.size() == 1);
    CHECK(static_cast<std::uint32_t>(effects[0].words[3]) == 0x19000000);
    gl::game::age_effects(effects, 25);
    CHECK(effects.empty());
}

TEST_CASE("real slot16: DEX effect reduces the sheet primary" *
          doctest::skip(!game_dir_available())) {
    const auto body = hero_body(16);
    auto stats = gl::assets::parse_hero_stats(body);
    REQUIRE(stats.has_value());
    auto effect_body = gl::assets::parse_hero_save_body(body);
    REQUIRE(effect_body.has_value());
    REQUIRE(effect_body->extra.size() == 1);

    gl::game::StatInputs inputs;
    inputs.strength = stats->strength;
    inputs.constitution = stats->constitution;
    inputs.dexterity = stats->dexterity;
    inputs.perception = stats->perception;
    inputs.luck = stats->luck;
    inputs.wisdom = stats->wisdom;
    inputs.intelligence = stats->intelligence;
    inputs.experience = stats->experience;
    inputs.skills.fill(0);
    for (std::size_t id = 0; id < inputs.skills.size(); ++id)
        inputs.skills[id] = stats->skills[id];
    CHECK(inputs.dexterity == 10);

    const auto without = gl::game::compute_armor_weight(inputs);
    CHECK(without.primary[2] == 10);

    const auto effect = effect_at(effect_body->extra[0]);
    const auto mods = gl::game::active_effect_mods({effect});
    REQUIRE(mods.size() == 1);
    inputs.effect_mods = mods;
    const auto with = gl::game::compute_armor_weight(inputs);
    CHECK(with.primary[2] == 7);
}
