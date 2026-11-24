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

const gl::game::CharacterRow &skill_at(const gl::game::CharacterSheet &sheet,
                                       std::size_t index) {
    return sheet.skill_groups[index / 9].rows[index % 9];
}
} // namespace

TEST_CASE("character sheet maps primary attributes in UI order") {
    const auto runtime = make_runtime();
    const gl::game::CharacterSheet sheet =
        gl::game::build_character_sheet(runtime, ui_text, "Вертас");
    REQUIRE(sheet.primary.size() == 7);
    CHECK(sheet.primary[0].label == "L6");
    CHECK(sheet.primary[0].value == "30");
    CHECK(sheet.primary[1].label == "L7");
    CHECK(sheet.primary[1].value == "25");
    CHECK(sheet.primary[2].label == "L8");
    CHECK(sheet.primary[2].value == "18");
    CHECK(sheet.primary[3].label == "L9");
    CHECK(sheet.primary[3].value == "12");
    CHECK(sheet.primary[4].label == "L10");
    CHECK(sheet.primary[4].value == "7");
    CHECK(sheet.primary[5].label == "L11");
    CHECK(sheet.primary[5].value == "5");
    CHECK(sheet.primary[6].label == "L12");
    CHECK(sheet.primary[6].value == "9");
    CHECK(sheet.experience_points.label == "L13");
    CHECK(sheet.experience_points.value == "0");
}

TEST_CASE("character sheet maps 3 skill groups through the original permutation") {
    constexpr std::array<std::uint32_t, 27> kPermutation{
        12, 13, 7,  8,  9,  10, 11, 14, 24, 4,  5,  0,  1,  2,
        3,  17, 25, 23, 26, 15, 20, 21, 16, 18, 19, 22, 6};
    gl::assets::HeroStats stats;
    for (std::uint32_t id = 0; id < 27; ++id)
        stats.skills[id] = static_cast<double>(id);
    const auto runtime = gl::game::PersonRuntime::create({}, stats);
    const auto sheet = gl::game::build_character_sheet(runtime, ui_text, "Вертас");
    REQUIRE(sheet.skill_groups.size() == 3);
    for (const auto &group : sheet.skill_groups)
        REQUIRE(group.rows.size() == 9);
    CHECK(sheet.skill_groups[0].title == "L83");
    CHECK(sheet.skill_groups[1].title == "L84");
    CHECK(sheet.skill_groups[2].title == "L85");
    for (std::size_t index = 0; index < kPermutation.size(); ++index) {
        const auto &row = skill_at(sheet, index);
        CHECK(row.label == "L" + std::to_string(49 + index));
        const std::uint32_t expected = kPermutation[index] > 15 ? 15 : kPermutation[index];
        CHECK(row.value == std::to_string(expected));
    }
}

TEST_CASE("character sheet state, sections and level") {
    const auto runtime = make_runtime();
    const auto sheet = gl::game::build_character_sheet(runtime, ui_text, "Вертас");
    REQUIRE(sheet.state.size() == 6);
    CHECK(sheet.state[0].label == "L14");
    CHECK(sheet.state[0].value == "40");
    CHECK(sheet.state[1].label == "L15");
    CHECK(sheet.state[1].value == "81099/82000");
    CHECK(sheet.state[2].label == "L16");
    CHECK(sheet.state[2].value == "-40");
    CHECK(sheet.state[3].label == "L27");
    CHECK(sheet.state[3].value == "200/216");
    CHECK(sheet.state[4].label == "L28");
    CHECK(sheet.state[4].value == "11/38");
    CHECK_FALSE(sheet.state[5].modeled);
    CHECK(sheet.state[5].value == "—");
    CHECK(sheet.experience_max == 82000);
    REQUIRE(sheet.combat.size() == 10);
    REQUIRE(sheet.resistance.size() == 12);
    REQUIRE(sheet.immunity.size() == 6);
    for (const auto &row : sheet.combat)
        CHECK_FALSE(row.modeled);
    CHECK(sheet.combat[0].label == "L19");
    CHECK(sheet.resistance[0].label == "L30");
    CHECK(sheet.immunity[0].label == "L43");
    CHECK(sheet.skill_points == 0);
}

TEST_CASE("character sheet name and unknown person") {
    const auto runtime = make_runtime();
    const auto hero = gl::game::build_character_sheet(runtime, ui_text, "Вертас");
    CHECK(hero.name == "Вертас");

    const auto missing =
        gl::game::build_character_sheet(runtime, ui_text, "", "missing");
    CHECK(missing.name == "missing");
    CHECK(missing.primary[0].value == "0");
    CHECK(missing.skill_groups[0].rows[0].value == "0");
}

TEST_CASE("character sheet falls back to #id for missing UI text") {
    const auto runtime = make_runtime();
    const auto sheet = gl::game::build_character_sheet(
        runtime, [](std::uint32_t) { return std::string{}; }, "Вертас");
    CHECK(sheet.primary[0].label == "#6");
    CHECK(sheet.skill_groups[0].rows[0].label == "#49");
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
    const auto sheet = gl::game::build_character_sheet(runtime, sdb.value(), "Вертас");
    CHECK(sheet.name == "Вертас");
    CHECK(sheet.primary[0].value == "30");
    CHECK(sheet.primary[1].value == "25");
    CHECK(sheet.primary[2].value == "18");
    CHECK(sheet.primary[3].value == "12");
    CHECK(sheet.primary[4].value == "7");
    CHECK(sheet.primary[5].value == "5");
    CHECK(sheet.primary[6].value == "9");
    CHECK(sheet.state[1].value == "81099/82000");
    CHECK(skill_at(sheet, 0).value == "0");
    CHECK(skill_at(sheet, 1).value == "15");
    CHECK(skill_at(sheet, 6).value == "0");
    CHECK(skill_at(sheet, 21).value == "0");
}
