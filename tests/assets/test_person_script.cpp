#include "gl/assets/person_script.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <span>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
std::span<const std::uint8_t> as_bytes(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}
} // namespace

TEST_CASE("person script: parses res_name, level and top-level values") {
    const std::string text = "// person header\n"
                             "res_name:\t\"L0.P1_Test\"\r\n"
                             "level: 20\n"
                             "strength: 14\n"
                             "title: \"Ivan\" // display name\n"
                             "weapon:\n"
                             "{\n"
                             "  res_name: \"ignored\"\n"
                             "  level: 99\n"
                             "  NPC_TEST_AXE 18 200\n"
                             "}\n"
                             "skills: {\n"
                             "  SKILL_SWORD 3\n"
                             "}\n"
                             "magic:\n"
                             "{\n"
                             "  FIREBALL 1\n"
                             "}\n"
                             "ai:\n"
                             "{\n"
                             "  battle_magic_use: 0\n"
                             "  life_escape 20\n"
                             "}\n"
                             "trade_panel: {\n"
                             "  change: 1\n"
                             "  repair 1\n"
                             "}\n";
    auto result = parse_person_script(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->res_name == "L0.P1_Test");
    CHECK(result->level == 20);
    CHECK(result->values.at("strength") == "14");
    CHECK(result->values.at("title") == "Ivan");
    CHECK(result->values.at("weapon.res_name") == "ignored");
    CHECK(result->values.at("weapon.level") == "99");
    CHECK(result->values.at("weapon.NPC_TEST_AXE") == "18 200");
    CHECK(result->values.at("skills.SKILL_SWORD") == "3");
    CHECK(result->values.at("magic.FIREBALL") == "1");
    CHECK(result->values.at("ai.battle_magic_use") == "0");
    CHECK(result->values.at("ai.life_escape") == "20");
    CHECK(result->values.at("trade_panel.change") == "1");
    CHECK(result->values.at("trade_panel.repair") == "1");
    CHECK(result->values.find("res_name") == result->values.end());
    CHECK(result->values.find("level") == result->values.end());
    CHECK(result->values.find("weapon") == result->values.end());
    CHECK(result->values.find("skills") == result->values.end());
    CHECK(result->values.find("magic") == result->values.end());
    CHECK(result->values.find("ai") == result->values.end());
    CHECK(result->values.find("trade_panel") == result->values.end());
}

TEST_CASE("person script: accepts keys without a colon") {
    const std::string text = "res_name \"L0.P2_Test\"\n"
                             "level 7\n"
                             "luck 3\n";
    auto result = parse_person_script(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->res_name == "L0.P2_Test");
    CHECK(result->level == 7);
    CHECK(result->values.at("luck") == "3");
}

TEST_CASE("person script: defaults level and requires res_name") {
    auto defaults = parse_person_script(as_bytes("res_name: \"L0.P3_Test\"\nstrength: 9\n"));
    REQUIRE(defaults.has_value());
    CHECK(defaults->level == 0);

    auto missing = parse_person_script(as_bytes("level: 5\nstrength: 9\n"));
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code == gl::ErrorCode::format);
}

TEST_CASE("person script: rejects an invalid level") {
    auto result = parse_person_script(as_bytes("res_name: \"L0.P4_Test\"\nlevel: strong\n"));
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == gl::ErrorCode::format);
}

TEST_CASE("person script: converts cp1251 values to UTF-8") {
    const std::string text = "res_name: \"L0.P5_Test\"\n"
                             "title: \"\xCF\xF0\xE8\xE2\xE5\xF2\"\n";
    auto result = parse_person_script(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->values.at("title") ==
          "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
}

TEST_CASE("person script: weapon block keeps file order and level offset") {
    const std::string text = "res_name: \"L0.P6_Test\"\n"
                             "level: 7\n"
                             "weapon:\n"
                             "{\n"
                             "  level_offset -3\n"
                             "  NPC_HANDS_A 10 20\n"
                             "  NPC_HANDS_B -100 10\n"
                             "  NPC_HANDS_C 1 3\n"
                             "}\n";
    auto result = parse_person_script(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->has_weapon);
    CHECK(result->weapon_level_offset == -3);
    REQUIRE(result->weapons.size() == 3);
    CHECK(result->weapons[0].name == "NPC_HANDS_A");
    CHECK(result->weapons[0].min_level == 10);
    CHECK(result->weapons[0].max_level == 20);
    CHECK(result->weapons[1].name == "NPC_HANDS_B");
    CHECK(result->weapons[2].name == "NPC_HANDS_C");
    CHECK(result->values.at("weapon.NPC_HANDS_A") == "10 20");
    CHECK(result->values.at("weapon.level_offset") == "-3");
}

TEST_CASE("person script: absent weapon block leaves weapon fields clear") {
    auto result = parse_person_script(
        as_bytes("res_name: \"L0.P7_Test\"\nlevel: 3\nstrength: 4\n"));
    REQUIRE(result.has_value());
    CHECK_FALSE(result->has_weapon);
    CHECK(result->weapons.empty());
    CHECK(result->weapon_level_offset == 0);
}

TEST_CASE("person script: inline weapon block skips non-numeric rows") {
    const std::string text = "res_name: \"L0.P8_Test\"\n"
                             "level: 5\n"
                             "weapon: {\n"
                             "  res_name: \"ignored\"\n"
                             "  level: 99\n"
                             "  NPC_OK 1 10 // comment\n"
                             "}\n";
    auto result = parse_person_script(as_bytes(text));
    REQUIRE(result.has_value());
    CHECK(result->has_weapon);
    REQUIRE(result->weapons.size() == 1);
    CHECK(result->weapons[0].name == "NPC_OK");
    CHECK(result->weapons[0].min_level == 1);
    CHECK(result->weapons[0].max_level == 10);
}
