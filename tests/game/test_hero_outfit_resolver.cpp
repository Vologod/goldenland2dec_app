#include "gl/assets/bytes.hpp"
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/herowear.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/hero_outfit_resolver.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace gl;
using namespace gl::game;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

void add_u32(assets::Bytes& bytes, std::uint32_t value) {
    for (int i = 0; i < 4; ++i)
        bytes.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
}

assets::Bytes make_sdb(std::initializer_list<std::pair<std::uint32_t, std::string>> entries) {
    assets::Bytes data = {'S', 'D', 'B', ' '};
    for (const auto& [id, text] : entries) {
        add_u32(data, id);
        add_u32(data, static_cast<std::uint32_t>(text.size()));
        data.insert(data.end(), text.begin(), text.end());
    }
    return data;
}

assets::HeroEquipment equipment(std::initializer_list<assets::EquippedItem> items) {
    assets::HeroEquipment result;
    result.items.assign(items.begin(), items.end());
    return result;
}

assets::HeroWearCatalog slot_catalog() {
    assets::HeroWearCatalog catalog;
    catalog.entries = {
        {"AXE_1_13_2", "weapon", "Axe_goluboy2", "two_axes_maces"},
        {"SWR_1_08_1", "weapon", "Sword_platina", "onehand_wpn"},
        {"ARM_1_13_1", "armor", "Dospeh_almaz", ""},
        {"ARM_1_12_2", "armor", "Dospeh_cherny", ""},
        {"HLM_1_12_1", "helmet", "Shlem_goluboy", ""},
        {"HLM_1_11_1", "helmet", "Shlem_goluboy", ""},
        {"ELB_1_11_2", "elbarms", "Ponogi_goluboy", ""},
        {"ELB_1_11_1", "elbarms", "Ponogi_goluboy", ""},
        {"SHL_1_10_1", "shield", "Shit_bio", ""},
        {"SHL_1_12_2", "shield", "Shit_cherny", ""},
    };
    return catalog;
}

assets::Bytes read_save(int slot) {
    const std::filesystem::path path =
        std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
        ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

const assets::HeroWearEntry* find_tech(const assets::HeroWearCatalog& catalog,
                                       std::string_view tech) {
    return catalog.find(tech);
}
} // namespace

TEST_CASE("resolve_hero_outfit uses set A and shared armor categories") {
    auto sdb = assets::Sdb::parse(make_sdb({{486, "AXE_1_13_2"},
                                            {554, "SWR_1_08_1"},
                                            {318, "ARM_1_13_1"},
                                            {280, "HLM_1_12_1"},
                                            {397, "ELB_1_11_2"},
                                            {353, "SHL_1_10_1"}}));
    REQUIRE(sdb.has_value());

    const auto items = equipment({
        {486, 1, 0x001000, 13},
        {554, 0, 0x002000, 8},
        {318, 12, 0x100000, 13},
        {280, 11, 0x004000, 12},
        {397, 14, 0x200000, 11},
        {353, 13, 0x800000, 10},
    });
    auto outfit = resolve_hero_outfit(items, sdb.value(), slot_catalog());
    REQUIRE(outfit.has_value());
    CHECK(outfit->wear_set == "two_axes_maces");
    CHECK(outfit->weapon == "axe_goluboy2");
    CHECK(outfit->shield.empty());
    CHECK(outfit->armor == "dospeh_almaz");
    CHECK(outfit->helmet == "shlem_goluboy");
    CHECK(outfit->elbarms == "ponogi_goluboy");
}

TEST_CASE("resolve_hero_outfit uses a one-handed base set and set A shield") {
    auto sdb = assets::Sdb::parse(
        make_sdb({{486, "AXE_1_13_2"}, {554, "SWR_1_08_1"}, {353, "SHL_1_10_1"}}));
    REQUIRE(sdb.has_value());

    const auto items = equipment({
        {486, 1, 0x002000, 13},
        {554, 0, 0x001000, 8},
        {353, 13, 0x400000, 10},
    });
    auto outfit = resolve_hero_outfit(items, sdb.value(), slot_catalog());
    REQUIRE(outfit.has_value());
    CHECK(outfit->wear_set == "onehand_wpn");
    CHECK(outfit->weapon == "sword_platina");
    CHECK(outfit->shield == "shit_bio");
}

TEST_CASE("resolve_hero_outfit falls back to naked per missing category") {
    auto sdb = assets::Sdb::parse(make_sdb({{318, "ARM_1_13_1"}, {999, "UNKNOWN_1"}}));
    REQUIRE(sdb.has_value());

    const auto items = equipment({{318, 12, 0x100000, 13}, {999, 1, 0x001000, 0}});
    auto outfit = resolve_hero_outfit(items, sdb.value(), slot_catalog());
    REQUIRE(outfit.has_value());
    CHECK(outfit->wear_set == "noweapon_thrw");
    CHECK(outfit->weapon.empty());
    CHECK(outfit->shield.empty());
    CHECK(outfit->armor == "dospeh_almaz");
    CHECK(outfit->helmet.empty());
    CHECK(outfit->elbarms.empty());
}

TEST_CASE("resolve_hero_outfit leaves a category empty when its TECH has no group") {
    auto sdb = assets::Sdb::parse(
        make_sdb({{123, "ARM_MISSING_1"}, {124, "AXE_1_13_2"}}));
    REQUIRE(sdb.has_value());

    auto outfit = resolve_hero_outfit(equipment({{123, 12, 0x100000, 0}}), sdb.value(),
                                      slot_catalog());
    REQUIRE(outfit.has_value());
    CHECK(outfit->wear_set == "noweapon_thrw");
    CHECK(outfit->armor.empty());

    auto mismatched = resolve_hero_outfit(equipment({{124, 12, 0x100000, 0}}), sdb.value(),
                                          slot_catalog());
    REQUIRE(mismatched.has_value());
    CHECK(mismatched->wear_set == "noweapon_thrw");
    CHECK(mismatched->armor.empty());
    CHECK(mismatched->weapon.empty());
}

TEST_CASE("resolve_hero_outfit ignores accessories and set B armor duplicates") {
    auto sdb = assets::Sdb::parse(
        make_sdb({{72, "BRL_X"}, {233, "RNG_X"}, {141, "AML_X"}, {318, "ARM_1_13_1"}}));
    REQUIRE(sdb.has_value());

    auto catalog = slot_catalog();
    catalog.entries.push_back({"BRL_X", "bracelet", "Bracelet_x", ""});
    catalog.entries.push_back({"RNG_X", "ring", "Ring_x", ""});
    catalog.entries.push_back({"AML_X", "amulet", "Amulet_x", ""});

    const auto items = equipment({
        {72, 15, 0x040000, 11},
        {233, 17, 0x020000, 10},
        {141, 16, 0x008000, 7},
        {318, 12, 0x100000, 13},
    });
    auto outfit = resolve_hero_outfit(items, sdb.value(), catalog);
    REQUIRE(outfit.has_value());
    CHECK(outfit->weapon.empty());
    CHECK(outfit->shield.empty());
    CHECK(outfit->armor == "dospeh_almaz");
}

TEST_CASE("real herowear and tech_names map equipped items" *
          doctest::skip(!game_dir_available())) {
    auto vfs = assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());

    auto sdb_data = vfs->read("sdb/items/tech_names.sdb");
    REQUIRE(sdb_data.has_value());
    auto sdb = assets::Sdb::parse(sdb_data.value());
    REQUIRE(sdb.has_value());
    REQUIRE(sdb->find(318) != nullptr);
    CHECK(*sdb->find(318) == "ARM_1_13_1");
    REQUIRE(sdb->find(397) != nullptr);
    CHECK(*sdb->find(397) == "ELB_1_11_2");
    REQUIRE(sdb->find(280) != nullptr);
    CHECK(*sdb->find(280) == "HLM_1_12_1");

    auto wear_data = vfs->read("scripts/herowear.scr");
    REQUIRE(wear_data.has_value());
    auto wear = assets::parse_herowear(wear_data.value());
    REQUIRE(wear.has_value());
    CHECK(wear->entries.size() == 637);
    const assets::HeroWearEntry* armor = find_tech(wear.value(), "ARM_1_13_1");
    REQUIRE(armor != nullptr);
    CHECK(armor->group == "Dospeh_almaz");
    const assets::HeroWearEntry* axe = find_tech(wear.value(), "AXE_1_13_2");
    REQUIRE(axe != nullptr);
    CHECK(axe->base_set == "two_axes_maces");
    const assets::HeroWearEntry* helmet = find_tech(wear.value(), "HLM_1_12_1");
    REQUIRE(helmet != nullptr);
    CHECK(helmet->group == "Shlem_goluboy");
    CHECK(find_tech(wear.value(), "SHLEM_CHERNY") == nullptr);

    const std::pair<int, std::string> armor_groups[] = {{0, "dospeh_almaz"},
                                                        {2, "dospeh_cherny"}};
    for (const auto& [slot, expected_armor] : armor_groups) {
        auto body = assets::decode_save_body(read_save(slot));
        REQUIRE(body.has_value());
        auto eq = assets::parse_hero_equipment(body.value());
        REQUIRE(eq.has_value());
        auto outfit = resolve_hero_outfit(eq.value(), sdb.value(), wear.value());
        REQUIRE(outfit.has_value());
        CHECK(outfit->wear_set == "two_axes_maces");
        CHECK(outfit->weapon == "axe_goluboy2");
        CHECK(outfit->armor == expected_armor);
        CHECK(outfit->helmet == "shlem_goluboy");
        CHECK(outfit->elbarms == "ponogi_goluboy");

        const std::string prefix = "wear/" + outfit->wear_set + "/";
        CHECK(vfs->exists(prefix + "weapon/" + outfit->weapon + "/" + outfit->weapon + ".iad"));
        CHECK(vfs->exists(prefix + "armor/" + outfit->armor + "/" + outfit->armor + ".iad"));
        CHECK(vfs->exists(prefix + "helmet/" + outfit->helmet + "/" + outfit->helmet + ".iad"));
        CHECK(vfs->exists(prefix + "elbarms/" + outfit->elbarms + "/" + outfit->elbarms +
                          ".iad"));
    }
}

TEST_CASE("real slot0/slot2 set A and B wear assets all exist" *
          doctest::skip(!game_dir_available())) {
    auto vfs = assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());

    const char* assets[] = {
        "wear/two_axes_maces/weapon/axe_goluboy2/axe_goluboy2.iad",
        "wear/two_axes_maces/armor/dospeh_almaz/dospeh_almaz.iad",
        "wear/two_axes_maces/helmet/shlem_goluboy/shlem_goluboy.iad",
        "wear/two_axes_maces/elbarms/ponogi_goluboy/ponogi_goluboy.iad",
        "wear/onehand_wpn/weapon/sword_platina/sword_platina.iad",
        "wear/onehand_wpn/armor/dospeh_almaz/dospeh_almaz.iad",
        "wear/onehand_wpn/shield/shit_bio/shit_bio.iad",
        "wear/onehand_wpn/helmet/shlem_goluboy/shlem_goluboy.iad",
        "wear/onehand_wpn/elbarms/ponogi_goluboy/ponogi_goluboy.iad",
        "wear/two_axes_maces/weapon/axe_goluboy2/axe_goluboy2.iad",
        "wear/two_axes_maces/armor/dospeh_cherny/dospeh_cherny.iad",
        "wear/two_axes_maces/helmet/shlem_goluboy/shlem_goluboy.iad",
        "wear/two_axes_maces/elbarms/ponogi_goluboy/ponogi_goluboy.iad",
        "wear/onehand_wpn/weapon/sword_platina/sword_platina.iad",
        "wear/onehand_wpn/armor/dospeh_cherny/dospeh_cherny.iad",
        "wear/onehand_wpn/shield/shit_cherny/shit_cherny.iad",
        "wear/onehand_wpn/helmet/shlem_goluboy/shlem_goluboy.iad",
        "wear/onehand_wpn/elbarms/ponogi_goluboy/ponogi_goluboy.iad",
    };
    int found = 0;
    for (const char* path : assets) {
        CHECK(vfs->exists(path));
        if (vfs->exists(path))
            ++found;
    }
    CHECK(found == 18);
}
