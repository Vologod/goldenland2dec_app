#include <doctest/doctest.h>
#include "gl/game/character_sheet.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/script/person_access.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {
gl::game::PersonRuntime make_runtime() {
    gl::assets::HeroStats stats;
    stats.strength = 30;
    stats.constitution = 25;
    stats.dexterity = 18;
    stats.perception = 12;
    stats.intelligence = 5;
    stats.wisdom = 7;
    stats.luck = 9;
    stats.reputation = -40;
    stats.experience = 81099;
    stats.life = 200;
    stats.energy = 11;
    stats.skills[21] = 1;
    stats.skills[12] = 15;
    return gl::game::PersonRuntime::create({}, stats);
}

std::string ui_text(std::uint32_t id) { return "L" + std::to_string(id); }
} // namespace

TEST_CASE("character sheet maps primary attributes in UI order") {
    const auto runtime = make_runtime();
    const gl::game::CharacterSheet sheet =
        gl::game::build_character_sheet(runtime, ui_text);
    REQUIRE(sheet.primary.size() == 8);
    CHECK(sheet.primary[0].label == "L6");
    CHECK(sheet.primary[0].value == "30");
    CHECK(sheet.primary[1].label == "L7");
    CHECK(sheet.primary[1].value == "25");
    CHECK(sheet.primary[3].label == "L9");
    CHECK(sheet.primary[3].value == "12");
    CHECK(sheet.primary[4].label == "L10");
    CHECK(sheet.primary[4].value == "7");
    CHECK(sheet.primary[5].label == "L11");
    CHECK(sheet.primary[5].value == "5");
    CHECK(sheet.primary[7].label == "L16");
    CHECK(sheet.primary[7].value == "-40");
}

TEST_CASE("character sheet maps the 27 skills through the original permutation") {
    constexpr std::array<std::uint32_t, 27> kPermutation{
        12, 13, 7,  8,  9,  10, 11, 14, 24, 4,  5,  0,  1,  2,
        3,  17, 25, 23, 26, 15, 20, 21, 16, 18, 19, 22, 6};
    gl::assets::HeroStats stats;
    for (std::uint32_t id = 0; id < 27; ++id)
        stats.skills[id] = static_cast<double>(id);
    const auto runtime = gl::game::PersonRuntime::create({}, stats);
    const gl::game::CharacterSheet sheet =
        gl::game::build_character_sheet(runtime, ui_text);
    REQUIRE(sheet.skills.size() == 27);
    for (std::size_t index = 0; index < kPermutation.size(); ++index) {
        CHECK(sheet.skills[index].label == "L" + std::to_string(49 + index));
        const std::uint32_t expected = kPermutation[index] > 15 ? 15 : kPermutation[index];
        CHECK(sheet.skills[index].value == std::to_string(expected));
    }
}

TEST_CASE("character sheet marks unmodeled characteristics and computes the level") {
    const auto runtime = make_runtime();
    const auto sheet = gl::game::build_character_sheet(runtime, ui_text);
    REQUIRE(sheet.characteristics.size() == 15);
    CHECK(sheet.characteristics[0].label == "L14");
    CHECK(sheet.characteristics[0].modeled);
    CHECK(sheet.characteristics[0].value == "40");
    CHECK(sheet.personality[1].value == "40");
    CHECK(sheet.characteristics[1].label == "L15");
    CHECK(sheet.characteristics[1].value == "81099");
    CHECK(sheet.characteristics[2].label == "L16");
    CHECK(sheet.characteristics[2].value == "-40");
    CHECK(sheet.characteristics[3].label == "L27");
    CHECK(sheet.characteristics[4].label == "L28");
    CHECK_FALSE(sheet.characteristics[5].modeled);
    CHECK(sheet.characteristics[5].value == "-");
    CHECK(sheet.characteristics[14].label == "L17");
    CHECK(sheet.skill_points == 0);
}

TEST_CASE("character sheet personality rows and unknown person") {
    const auto runtime = make_runtime();
    const auto hero = gl::game::build_character_sheet(runtime, ui_text);
    REQUIRE(hero.personality.size() == 4);
    CHECK(hero.name == "Hero");
    CHECK(hero.personality[0].label == "L5");
    CHECK(hero.personality[0].value == "Hero");
    CHECK(hero.personality[1].label == "L14");

    const auto missing = gl::game::build_character_sheet(runtime, ui_text, "missing");
    CHECK(missing.name == "missing");
    CHECK(missing.primary[0].value == "0");
    CHECK(missing.skills[0].value == "0");
}

TEST_CASE("character sheet falls back to #id for missing UI text") {
    const auto runtime = make_runtime();
    const auto sheet = gl::game::build_character_sheet(
        runtime, [](std::uint32_t) { return std::string{}; });
    CHECK(sheet.primary[0].label == "#6");
    CHECK(sheet.skills[0].label == "#49");
}

TEST_CASE("character sheet reads real slot0" * doctest::skip(std::getenv("GL_GAME_DIR") == nullptr)) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    const std::filesystem::path path =
        std::filesystem::path(dir) / "Saves" / "slot0.sav";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    auto body = gl::assets::decode_save_body(bytes);
    REQUIRE(body.has_value());
    auto stats = gl::assets::parse_hero_stats(body.value());
    REQUIRE(stats.has_value());
    auto sdb_data = vfs->read("sdb/user_interface.sdb");
    REQUIRE(sdb_data.has_value());
    auto sdb = gl::assets::Sdb::parse(sdb_data.value());
    REQUIRE(sdb.has_value());
    auto runtime = gl::game::PersonRuntime::create({}, stats.value());
    const auto sheet = gl::game::build_character_sheet(runtime, sdb.value());
    CHECK(sheet.primary[0].value == "30");
    CHECK(sheet.primary[1].value == "25");
    CHECK(sheet.primary[2].value == "18");
    CHECK(sheet.primary[3].value == "12");
    CHECK(sheet.primary[4].value == "7");
    CHECK(sheet.primary[5].value == "5");
    CHECK(sheet.primary[6].value == "9");
    CHECK(sheet.primary[7].value == "-40");
    CHECK(sheet.characteristics[1].value == "81099");
    CHECK(sheet.skills[0].value == "0");   // SWORD
    CHECK(sheet.skills[1].value == "15");  // AXE
    CHECK(sheet.skills[6].value == "0");   // THROW
    CHECK(sheet.skills[21].value == "0");  // TALKING
}
