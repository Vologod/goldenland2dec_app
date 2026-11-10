#include "gl/assets/bytes.hpp"
#include "gl/assets/specials.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>
#include <string_view>

using namespace gl::assets;

namespace {
Bytes to_bytes(std::string_view text) { return Bytes(text.begin(), text.end()); }

gl::Result<SpecialDatabase> parse(std::string_view text) {
    return parse_specials_scr(to_bytes(text));
}

constexpr std::string_view kNineSlots = "SPECF_VALUE_CHANGE_ABSOLUTE + 1 50\n"
                                        "SPECF_VALUE_CHANGE_RELATIVE - 1 1\n"
                                        "SPECF_VALUE_REPLACE - 1 1\n"
                                        "SPECF_TIME_PERMANENT + 0.3 1\n"
                                        "SPECF_TIME_DAY - 0.3 0.5\n"
                                        "SPECF_TIME_NIGHT - 0.3 0.5\n"
                                        "SPECF_TIME_INTERVAL - 1 1\n"
                                        "SPECF_TIME_IMMEDIATE - 1 1\n"
                                        "SPECF_TIME_DELAYED - 1 1\n";

std::string def_block(std::string_view id, bool random_sign) {
    std::string text = "special_desc ";
    text += id;
    if (random_sign)
        text += " random_sign";
    text += "\n{\n";
    text += kNineSlots;
    text += "}\n";
    return text;
}
} // namespace

TEST_CASE("specials: parses defs, slots, random_sign and item classes") {
    std::string text = def_block("IDSPEC_POISON_DMG", false);
    text += def_block("IDSPEC_COLD_RES", true);
    text += "itemclass T_SWORD\n"
            "special IDSPEC_COLD_RES\n"
            "special IDSPEC_POISON_DMG\n"
            "special IDSPEC_POISON_DMG\n"
            "itemclass T_HELM\n"
            "special IDSPEC_COLD_RES\n";

    auto db = parse(text);
    REQUIRE(db.has_value());
    REQUIRE(db->defs.size() == 2);
    REQUIRE(db->item_classes.size() == 2);

    const SpecialDef &poison = db->defs[0];
    CHECK(poison.id == 0);
    CHECK_FALSE(poison.random_sign);
    REQUIRE(poison.value_slots.size() == special_value_slot_count);
    REQUIRE(poison.time_slots.size() == special_time_slot_count);
    CHECK(poison.value_slots[0].tag == 0x40);
    CHECK(poison.value_slots[0].access == 1);
    CHECK(poison.value_slots[0].value == doctest::Approx(1.0f));
    CHECK(poison.value_slots[0].price == doctest::Approx(50.0f));
    CHECK(poison.value_slots[1].tag == 0x80);
    CHECK(poison.value_slots[1].access == 0);
    CHECK(poison.time_slots[0].tag == 1);
    CHECK(poison.time_slots[0].access == 1);
    CHECK(poison.time_slots[0].value == doctest::Approx(0.3f));
    CHECK(poison.time_slots[0].price == doctest::Approx(1.0f));
    CHECK(poison.time_slots[5].tag == 0x20);

    const SpecialDef &cold = db->defs[1];
    CHECK(cold.id == 4);
    CHECK(cold.random_sign);

    const auto &swords = db->item_classes.at(ItemType::sword);
    REQUIRE(swords.size() == 2);
    CHECK(swords[0] == 4);
    CHECK(swords[1] == 0);

    const auto &helms = db->item_classes.at(ItemType::helmet);
    REQUIRE(helms.size() == 1);
    CHECK(helms[0] == 4);

    REQUIRE(db->find(0) != nullptr);
    CHECK(db->find(0)->id == 0);
    CHECK(db->find(1) == nullptr);
}

TEST_CASE("specials: keywords and names are case-insensitive") {
    std::string text = "SpEcIaL_DeSc idspEc_poison_dmg RaNdOm_SiGn\n"
                       "{\n";
    text += "specf_value_change_absolute + 1 50\n"
            "SPECF_VALUE_CHANGE_RELATIVE - 1 1\n"
            "SPECF_VALUE_REPLACE - 1 1\n"
            "SPECF_TIME_PERMANENT + 0.3 1\n"
            "SPECF_TIME_DAY - 0.3 0.5\n"
            "SPECF_TIME_NIGHT - 0.3 0.5\n"
            "SPECF_TIME_INTERVAL - 1 1\n"
            "SPECF_TIME_IMMEDIATE - 1 1\n"
            "SPECF_TIME_DELAYED - 1 1\n"
            "}\n"
            "ITEMCLASS t_sword\n"
            "SPECIAL idspec_poison_dmg\n";
    auto db = parse(text);
    REQUIRE(db.has_value());
    REQUIRE(db->defs.size() == 1);
    CHECK(db->defs[0].id == 0);
    CHECK(db->defs[0].random_sign);
    REQUIRE(db->item_classes.contains(ItemType::sword));
    CHECK(db->item_classes.at(ItemType::sword)[0] == 0);
}

TEST_CASE("specials: partial records at EOF or '}' are accepted") {
    auto eof = parse("special_desc IDSPEC_POISON_DMG\n{\n"
                     "SPECF_VALUE_CHANGE_ABSOLUTE + 1 50\n"
                     "SPECF_TIME_PERMANENT + 0.3 1\n");
    REQUIRE(eof.has_value());
    REQUIRE(eof->defs.size() == 1);
    CHECK(eof->defs[0].value_slots[0].tag == 0x40);
    CHECK(eof->defs[0].value_slots[0].value == doctest::Approx(1.0f));
    CHECK(eof->defs[0].value_slots[1].tag == 0);
    CHECK(eof->defs[0].time_slots[0].tag == 1);
    CHECK(eof->defs[0].time_slots[1].tag == 0);

    auto closed = parse("special_desc IDSPEC_COLD_RES\n{\n"
                        "SPECF_VALUE_CHANGE_ABSOLUTE + 2 60\n"
                        "SPECF_TIME_PERMANENT + 0.5 2\n"
                        "}\n");
    REQUIRE(closed.has_value());
    REQUIRE(closed->defs.size() == 1);
    CHECK(closed->defs[0].value_slots[0].value == doctest::Approx(2.0f));
    CHECK(closed->defs[0].time_slots[0].value == doctest::Approx(0.5f));
    CHECK(closed->defs[0].value_slots[1].tag == 0);
    CHECK(closed->defs[0].time_slots[1].tag == 0);
}

TEST_CASE("specials: duplicate tags and full categories are ignored") {
    auto db = parse("special_desc IDSPEC_POISON_DMG\n{\n"
                    "SPECF_TIME_PERMANENT + 0.3 1\n"
                    "SPECF_VALUE_CHANGE_ABSOLUTE + 1 50\n"
                    "SPECF_VALUE_CHANGE_ABSOLUTE + 9 99\n"
                    "SPECF_VALUE_CHANGE_RELATIVE + 2 2\n"
                    "SPECF_VALUE_REPLACE + 3 3\n"
                    "SPECF_VALUE_REPLACE + 4 4\n"
                    "SPECF_TIME_DAY - 0.3 0.5\n"
                    "SPECF_TIME_NIGHT - 0.3 0.5\n"
                    "SPECF_TIME_INTERVAL - 1 1\n"
                    "SPECF_TIME_IMMEDIATE - 1 1\n"
                    "SPECF_TIME_DELAYED - 1 1\n"
                    "SPECF_TIME_PERMANENT + 9 99\n"
                    "}\n");
    REQUIRE(db.has_value());
    REQUIRE(db->defs.size() == 1);
    const SpecialDef &def = db->defs[0];
    const std::array<std::uint32_t, 3> value_tags = {0x40, 0x80, 0x100};
    const std::array<float, 3> value_values = {1.0f, 2.0f, 3.0f};
    for (std::size_t i = 0; i < value_tags.size(); ++i) {
        CHECK(def.value_slots[i].tag == value_tags[i]);
        CHECK(def.value_slots[i].value == doctest::Approx(value_values[i]));
    }
    const std::array<std::uint32_t, 6> time_tags = {1, 2, 4, 8, 0x10, 0x20};
    for (std::size_t i = 0; i < time_tags.size(); ++i)
        CHECK(def.time_slots[i].tag == time_tags[i]);
    CHECK(def.time_slots[0].value == doctest::Approx(0.3f));
}

TEST_CASE("specials: access is one only for '+'") {
    auto db = parse("special_desc IDSPEC_POISON_DMG\n{\n"
                    "SPECF_VALUE_CHANGE_ABSOLUTE * 1 50\n"
                    "SPECF_VALUE_CHANGE_RELATIVE x 1 1\n"
                    "SPECF_VALUE_REPLACE - 1 1\n"
                    "SPECF_TIME_PERMANENT - 0.3 1\n"
                    "SPECF_TIME_DAY + 0.3 0.5\n"
                    "}\n");
    REQUIRE(db.has_value());
    REQUIRE(db->defs.size() == 1);
    CHECK(db->defs[0].value_slots[0].access == 0);
    CHECK(db->defs[0].value_slots[1].access == 0);
    CHECK(db->defs[0].value_slots[2].access == 0);
    CHECK(db->defs[0].time_slots[0].access == 0);
    CHECK(db->defs[0].time_slots[1].access == 1);
}

TEST_CASE("specials: SPECF_ANTI_MONSTER_SPECIFIED is rejected in slot context") {
    auto db = parse("special_desc IDSPEC_POISON_DMG\n{\n"
                    "SPECF_ANTI_MONSTER_SPECIFIED + 1 50\n"
                    "}\n");
    REQUIRE_FALSE(db.has_value());
    CHECK(db.error().code == gl::ErrorCode::format);
}

TEST_CASE("specials: error text converts cp1251 tokens to UTF-8") {
    std::string text = "special_desc ";
    text += "\xCF\xD0"; // cp1251 "ПР"
    text += "\n{\n}\n";
    auto db = parse(text);
    REQUIRE_FALSE(db.has_value());
    CHECK(db.error().code == gl::ErrorCode::format);
    CHECK(db.error().message.find("\xD0\x9F\xD0\xA0") != std::string::npos);
}

TEST_CASE("specials: supports comments and empty input") {
    auto db = parse("// leading comment\n"
                    "\n"
                    "// another comment\n");
    REQUIRE(db.has_value());
    CHECK(db->defs.empty());
    CHECK(db->item_classes.empty());

    std::string text = "// before\n";
    text += def_block("IDSPEC_FIRE_DMG", false); // trailing comment
    text += "itemclass T_SWORD // class comment\n"
            "special IDSPEC_FIRE_DMG // pool comment\n";
    auto commented = parse(text);
    REQUIRE(commented.has_value());
    REQUIRE(commented->defs.size() == 1);
    CHECK(commented->defs[0].id == 2);
}

TEST_CASE("specials: hardcoded IDSPEC map matches GoldenLand.exe") {
    const std::array<std::string_view, 78> names = {"IDSPEC_POISON_DMG",
                                                    "IDSPEC_COLD_DMG",
                                                    "IDSPEC_FIRE_DMG",
                                                    "IDSPEC_POISON_RES",
                                                    "IDSPEC_COLD_RES",
                                                    "IDSPEC_FIRE_RES",
                                                    "IDSPEC_STRENGTH",
                                                    "IDSPEC_CONSTITUTION",
                                                    "IDSPEC_PERCEPTION",
                                                    "IDSPEC_DEXTERITY",
                                                    "IDSPEC_INTELLIGENCE",
                                                    "IDSPEC_WISDOM",
                                                    "IDSPEC_LUCK",
                                                    "IDSPEC_GODS_MAGIC_RES",
                                                    "IDSPEC_ELEMENTS_MAGIC_RES",
                                                    "IDSPEC_LIGHTNESS_MAGIC_RES",
                                                    "IDSPEC_DARKNESS_MAGIC_RES",
                                                    "IDSPEC_SHADOWS_MAGIC_RES",
                                                    "IDSPEC_NATURE_MAGIC_RES",
                                                    "IDSPEC_HACKING_RES",
                                                    "IDSPEC_CRUSHING_RES",
                                                    "IDSPEC_PRICKING_RES",
                                                    "IDSPEC_MAX_HEALTH",
                                                    "IDSPEC_MAX_ENERGY",
                                                    "IDSPEC_INITIATIVE",
                                                    "IDSPEC_HEALTH_REGENERATE_TIME",
                                                    "IDSPEC_ENERGY_REGENERATE_TIME",
                                                    "IDSPEC_ACTION_POINTS",
                                                    "IDSPEC_REPUTATION",
                                                    "IDSPEC_ARMOR_CLASS",
                                                    "IDSPEC_MAX_WEIGHT_CARRIED",
                                                    "IDSPEC_TRANSFER_HEALTH",
                                                    "IDSPEC_TRANSFER_ENERGY",
                                                    "IDSPEC_HACKING_DMG",
                                                    "IDSPEC_CRUSHING_DMG",
                                                    "IDSPEC_PRICKING_DMG",
                                                    "IDSPEC_GODS_MAGIC_DMG",
                                                    "IDSPEC_ELEMENTS_MAGIC_DMG",
                                                    "IDSPEC_LIGHTNESS_MAGIC_DMG",
                                                    "IDSPEC_DARKNESS_MAGIC_DMG",
                                                    "IDSPEC_SHADOWS_MAGIC_DMG",
                                                    "IDSPEC_NATURE_MAGIC_DMG",
                                                    "IDSPEC_GODS_MAGIC_IMMUN",
                                                    "IDSPEC_ELEMENTS_MAGIC_IMMUN",
                                                    "IDSPEC_LIGHTNESS_MAGIC_IMMUN",
                                                    "IDSPEC_DARKNESS_MAGIC_IMMUN",
                                                    "IDSPEC_SHADOWS_MAGIC_IMMUN",
                                                    "IDSPEC_NATURE_MAGIC_IMMUN",
                                                    "IDSPEC_HEALTH_CURRENT",
                                                    "IDSPEC_ENERGY_CURRENT",
                                                    "IDSPEC_CHT_HIT",
                                                    "IDSPEC_CHT_CRITICAL_HIT",
                                                    "IDSPEC_MOD_CRITICAL_HIT",
                                                    "IDSPEC_CHT_CRITICAL_MISS",
                                                    "IDSPEC_CHT_DIALOG_LEVEL",
                                                    "IDSPEC_CHT_RUN_FROM_EVIL",
                                                    "IDSPEC_CHT_HIT_THROWING",
                                                    "IDSPEC_DMG_THROWING",
                                                    "IDSPEC_BROKEN_LEFT_FOOT",
                                                    "IDSPEC_BROKEN_RIGHT_FOOT",
                                                    "IDSPEC_BROKEN_LEFT_HAND",
                                                    "IDSPEC_BROKEN_RIGHT_HAND",
                                                    "IDSPEC_BLINDNESS",
                                                    "IDSPEC_STONECURSE",
                                                    "IDSPEC_FREEZE",
                                                    "IDSPEC_OGLUSHENIE",
                                                    "IDSPEC_RAGE",
                                                    "IDSPEC_SANCTITY",
                                                    "IDSPEC_DEATH_MIRROR",
                                                    "IDSPEC_IMBECILITY",
                                                    "IDSPEC_ELEMENTAL_SHIELD",
                                                    "IDSPEC_DISPELL",
                                                    "IDSPEC_MAP_WALKER",
                                                    "IDSPEC_SILENCE",
                                                    "IDSPEC_SUMMON_DEMISHADOW",
                                                    "IDSPEC_SUMMON_ICE_BEAST",
                                                    "IDSPEC_SUMMON_WINGED_DEMON",
                                                    "IDSPEC_SUMMON_VIOLIA"};

    CHECK(special_id_name_count() == names.size());
    for (std::size_t i = 0; i < names.size(); ++i) {
        CAPTURE(i);
        auto id = special_id_from_name(names[i]);
        REQUIRE(id.has_value());
        CHECK(*id == i);
    }
    CHECK_FALSE(special_id_from_name("IDSPEC_NOPE").has_value());
    CHECK_FALSE(special_id_from_name("").has_value());
}

TEST_CASE("specials: hardcoded SPECF map matches GoldenLand.exe") {
    CHECK(special_flag_name_count() == 10);
    CHECK(*special_flag_from_name("SPECF_TIME_PERMANENT") == 1);
    CHECK(*special_flag_from_name("SPECF_TIME_DAY") == 2);
    CHECK(*special_flag_from_name("SPECF_TIME_NIGHT") == 4);
    CHECK(*special_flag_from_name("SPECF_TIME_INTERVAL") == 8);
    CHECK(*special_flag_from_name("SPECF_TIME_IMMEDIATE") == 0x10);
    CHECK(*special_flag_from_name("SPECF_TIME_DELAYED") == 0x20);
    CHECK(*special_flag_from_name("SPECF_VALUE_CHANGE_ABSOLUTE") == 0x40);
    CHECK(*special_flag_from_name("SPECF_VALUE_CHANGE_RELATIVE") == 0x80);
    CHECK(*special_flag_from_name("SPECF_VALUE_REPLACE") == 0x100);
    CHECK(*special_flag_from_name("SPECF_ANTI_MONSTER_SPECIFIED") == 0x200);
    CHECK_FALSE(special_flag_from_name("SPECF_NOPE").has_value());
}

TEST_CASE("specials: hardcoded T_* map matches GoldenLand.exe") {
    CHECK(special_item_type_name_count() == 29);
    const std::array<std::pair<std::string_view, ItemType>, 29> types = {{
        {"T_SWORD", ItemType::sword},
        {"T_AXE", ItemType::axe},
        {"T_SPEAR", ItemType::spear},
        {"T_BOW", ItemType::bow},
        {"T_CROSSBOW", ItemType::crossbow},
        {"T_HANDGUNS", ItemType::rifle},
        {"T_CROSIER", ItemType::staff},
        {"T_MACE", ItemType::blunt},
        {"T_AMMO", ItemType::bullets},
        {"T_BOLTS", ItemType::bolts},
        {"T_ARROWS", ItemType::arrows},
        {"T_HELM", ItemType::helmet},
        {"T_ARMOUR", ItemType::cuirass},
        {"T_SHIELD", ItemType::shield},
        {"T_ELBARMS", ItemType::leggings},
        {"T_BRACELET", ItemType::bracers},
        {"T_AMULET", ItemType::amulet},
        {"T_RING", ItemType::ring},
        {"T_MONEY", ItemType::money},
        {"T_POTIONS", ItemType::potion},
        {"T_FOOD", ItemType::food},
        {"T_BOOK", ItemType::book},
        {"T_ROLLS", ItemType::scroll},
        {"T_MATERIAL", ItemType::smith_material},
        {"T_REAGENT", ItemType::alchemy_ingredient},
        {"T_QUEST_ITEM", ItemType::quest_item},
        {"T_RECIPE", ItemType::recipe},
        {"T_GARBAGE", ItemType::junk},
        {"T_STONE", ItemType::stone},
    }};
    for (const auto &[name, type] : types) {
        CAPTURE(name);
        auto parsed = special_item_type_from_name(name);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == type);
    }
    CHECK_FALSE(special_item_type_from_name("T_NOPE").has_value());
}

TEST_CASE("specials: unknown or malformed names are format errors") {
    auto require_format_error = [](const std::string &text) {
        auto db = parse(text);
        REQUIRE_FALSE(db.has_value());
        CHECK(db.error().code == gl::ErrorCode::format);
    };

    require_format_error("special_desc IDSPEC_NOPE\n{\n" + std::string(kNineSlots) + "}\n");
    require_format_error("special_desc IDSPEC_POISON_DMG\n{\n"
                         "SPECF_NOPE + 1 50\n"
                         "SPECF_VALUE_CHANGE_RELATIVE - 1 1\n"
                         "SPECF_VALUE_REPLACE - 1 1\n"
                         "SPECF_TIME_PERMANENT + 0.3 1\n"
                         "SPECF_TIME_DAY - 0.3 0.5\n"
                         "SPECF_TIME_NIGHT - 0.3 0.5\n"
                         "SPECF_TIME_INTERVAL - 1 1\n"
                         "SPECF_TIME_IMMEDIATE - 1 1\n"
                         "SPECF_TIME_DELAYED - 1 1\n"
                         "}\n");
    require_format_error("itemclass T_NOPE\n");
    require_format_error("itemclass T_SWORD\nspecial IDSPEC_NOPE\n");
}

TEST_CASE("specials: malformed grammar is a format error") {
    auto require_format_error = [](const std::string &text) {
        auto db = parse(text);
        REQUIRE_FALSE(db.has_value());
        CHECK(db.error().code == gl::ErrorCode::format);
    };

    require_format_error("special IDSPEC_POISON_DMG\n");
    require_format_error("special_desc IDSPEC_POISON_DMG } \n");
    require_format_error("special_desc IDSPEC_POISON_DMG\n{\nSPECF_VALUE_CHANGE_ABSOLUTE\n");
    require_format_error("special_desc IDSPEC_POISON_DMG\n{\nSPECF_VALUE_CHANGE_ABSOLUTE + 1\n");
    require_format_error(
        "special_desc IDSPEC_POISON_DMG\n{\nSPECF_VALUE_CHANGE_ABSOLUTE + nope 50\n" +
        std::string(kNineSlots).substr(kNineSlots.find('\n') + 1) + "}\n");
    require_format_error("special_desc IDSPEC_POISON_DMG random_sing\n{\n" +
                         std::string(kNineSlots) + "}\n");
    require_format_error("bogus_token\n");
}
