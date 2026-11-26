#include <doctest/doctest.h>

#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/item_catalog.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/character_sheet.hpp"
#include "gl/game/combat_stats.hpp"
#include "gl/game/person_runtime.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
using gl::assets::SpecialMod;
using gl::game::ArmorWeight;
using gl::game::CarriedItem;
using gl::game::EquippedStatItem;
using gl::game::StatInputs;

StatInputs base_inputs() {
    StatInputs in;
    in.strength = 30;
    in.constitution = 25;
    in.dexterity = 18;
    in.perception = 12;
    in.luck = 9;
    in.wisdom = 10;
    in.intelligence = 5;
    in.experience = 81099;
    in.skills.fill(0);
    in.skills[6] = 10;
    in.skills[13] = 15;
    in.skills[20] = 14;
    in.skills[22] = 1;
    in.skills[24] = 15;
    in.skills[26] = 10;
    return in;
}

EquippedStatItem shield_item(std::uint32_t armor, std::uint32_t durability) {
    EquippedStatItem item;
    item.type = 13;
    item.flags = 0x400000u;
    item.durability = durability;
    item.unknown_b = {0, 10, 0, armor, 0};
    return item;
}

SpecialMod mod(std::uint32_t id, std::int32_t value, std::uint32_t flags) {
    SpecialMod m;
    m.id = id;
    m.value = value;
    m.flags = flags;
    return m;
}

EquippedStatItem armor_piece(std::uint32_t mask, std::array<std::uint32_t, 6> damage) {
    EquippedStatItem piece;
    piece.type = 12;
    piece.flags = mask;
    piece.durability = 100;
    piece.damage = damage;
    return piece;
}

EquippedStatItem axe_weapon() {
    EquippedStatItem weapon;
    weapon.type = 1;
    weapon.flags = 0x001000u;
    weapon.durability = 100;
    weapon.damage = {47, 62, 72, 83, 0, 0};
    weapon.unknown_b = {20, 13, 8, 0, 70};
    weapon.mods.push_back(mod(1, 30, 0x41)); // cold typed bonus
    return weapon;
}

bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("combat stats: armor without shield uses dexterity and skill 6") {
    const StatInputs in = base_inputs();
    const ArmorWeight out = gl::game::compute_armor_weight(in);
    CHECK(out.armor_class == 9);
}

TEST_CASE("combat stats: armor adds shield unknown_b[3] scaled by durability") {
    StatInputs in = base_inputs();
    in.equipped.push_back(shield_item(20, 100));
    CHECK(gl::game::compute_armor_weight(in).armor_class == 29);
    in.equipped.clear();
    in.equipped.push_back(shield_item(20, 50));
    CHECK(gl::game::compute_armor_weight(in).armor_class == 24);
}

TEST_CASE("combat stats: set B shield is ignored for the active set A") {
    StatInputs in = base_inputs();
    EquippedStatItem other = shield_item(20, 100);
    other.flags = 0x800000u;
    in.equipped.push_back(other);
    CHECK(gl::game::compute_armor_weight(in).armor_class == 9);
}

TEST_CASE("combat stats: armor flat modifier (0x40) adds amount") {
    StatInputs in = base_inputs();
    in.equipped.push_back(shield_item(20, 100));
    EquippedStatItem ring;
    ring.flags = 0x8002006Cu;
    ring.mods.push_back(mod(0x1d, 5, 0x41));
    in.equipped.push_back(ring);
    CHECK(gl::game::compute_armor_weight(in).armor_class == 34);
}

TEST_CASE("combat stats: armor percent modifier (0x80) adds percent of base") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x1d, 50, 0x81));
    in.equipped.push_back(ring);
    // 9.352 + 50% of 9.352 = 14.028 -> 14
    CHECK(gl::game::compute_armor_weight(in).armor_class == 14);
}

TEST_CASE("combat stats: armor override takes max abs and ignores deltas") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x1d, 5, 0x41));
    ring.mods.push_back(mod(0x1d, 7, 0x100));
    ring.mods.push_back(mod(0x1d, -9, 0x100));
    in.equipped.push_back(ring);
    CHECK(gl::game::compute_armor_weight(in).armor_class == 0);
}

TEST_CASE("combat stats: conditional armor modifiers are skipped") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x1d, 5, 0x42));
    in.equipped.push_back(ring);
    CHECK(gl::game::compute_armor_weight(in).armor_class == 9);
}

TEST_CASE("combat stats: day modifier applies only at full day") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x1d, 5, 0x42));
    in.equipped.push_back(ring);
    in.condition.day_night_known = true;
    in.condition.day_night = 1.0;
    CHECK(gl::game::compute_armor_weight(in).armor_class == 14);
    in.condition.day_night = 0.0;
    CHECK(gl::game::compute_armor_weight(in).armor_class == 9);
    in.condition.day_night = 0.5;
    CHECK(gl::game::compute_armor_weight(in).armor_class == 9);
}

TEST_CASE("combat stats: night modifier applies only at full night") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x1d, 5, 0x44));
    in.equipped.push_back(ring);
    in.condition.day_night_known = true;
    in.condition.day_night = 0.0;
    CHECK(gl::game::compute_armor_weight(in).armor_class == 14);
    in.condition.day_night = 1.0;
    CHECK(gl::game::compute_armor_weight(in).armor_class == 9);
}

TEST_CASE("combat stats: day percent modifier applies only at full day") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x1d, 50, 0x82));
    in.equipped.push_back(ring);
    in.condition.day_night_known = true;
    in.condition.day_night = 1.0;
    CHECK(gl::game::compute_armor_weight(in).armor_class == 14);
    in.condition.day_night = 0.0;
    CHECK(gl::game::compute_armor_weight(in).armor_class == 9);
}

TEST_CASE("combat stats: time-interval and reject conditions are always skipped") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x1d, 5, 0x48));
    ring.mods.push_back(mod(0x1d, 5, 0x240));
    in.equipped.push_back(ring);
    in.condition.day_night_known = true;
    in.condition.day_night = 1.0;
    CHECK(gl::game::compute_armor_weight(in).armor_class == 9);
}

TEST_CASE("combat stats: armor clamps to 0..50") {
    StatInputs high = base_inputs();
    high.equipped.push_back(shield_item(2000, 100));
    CHECK(gl::game::compute_armor_weight(high).armor_class == 50);
}

TEST_CASE("combat stats: weight current sums count * weight_x10") {
    StatInputs in = base_inputs();
    in.carried.push_back(CarriedItem{2, 30});
    in.carried.push_back(CarriedItem{1, 45});
    CHECK(gl::game::compute_armor_weight(in).weight_cur_x10 == 105);
}

TEST_CASE("combat stats: weight max uses attributes, level and skill 6 bonus") {
    StatInputs in = base_inputs();
    // (30*20 + 25*20 + level 40) = 1140; skills[6]=10 >= 5 -> *1.5 = 1710
    CHECK(gl::game::compute_armor_weight(in).weight_max_x10 == 1710);
    in.skills[6] = 0;
    CHECK(gl::game::compute_armor_weight(in).weight_max_x10 == 1140);
}

TEST_CASE("combat stats: weight max flat modifier (0x1e) then *1.5") {
    StatInputs in = base_inputs();
    EquippedStatItem bag;
    bag.mods.push_back(mod(0x1e, 100, 0x40));
    in.equipped.push_back(bag);
    // (1140 + 100) * 1.5 = 1860
    CHECK(gl::game::compute_armor_weight(in).weight_max_x10 == 1860);
}

TEST_CASE("combat stats: weight max clamps to 1..9999") {
    StatInputs in = base_inputs();
    EquippedStatItem bag;
    bag.mods.push_back(mod(0x1e, 100000, 0x41));
    in.equipped.push_back(bag);
    CHECK(gl::game::compute_armor_weight(in).weight_max_x10 == 9999);
}

TEST_CASE("combat stats: weight capacity gets the person 0x2000 bonus") {
    StatInputs in = base_inputs();
    in.person_flags = 0x2000u;
    // 1140 * 1.5 * 1.1 = 1881
    CHECK(gl::game::compute_armor_weight(in).weight_max_x10 == 1881);
}

TEST_CASE("combat stats: attribute modifiers feed weight capacity") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(7, 2, 0x41)); // +2 constitution
    in.equipped.push_back(ring);
    // (30*20 + (25+2)*20 + 40) * 1.5 = 1770
    CHECK(gl::game::compute_armor_weight(in).weight_max_x10 == 1770);
}

TEST_CASE("combat stats: action points use skills, attributes and modifiers") {
    StatInputs in = base_inputs();
    // 10*(2/3) + (18 + 12/2) + 5 = 35.667 -> 36
    CHECK(gl::game::compute_armor_weight(in).action_points == 36);
    EquippedStatItem sword;
    sword.mods.push_back(mod(0x1b, 5, 0x41));
    in.equipped.push_back(sword);
    CHECK(gl::game::compute_armor_weight(in).action_points == 41);
}

TEST_CASE("combat stats: action points add 5 above skill 6 threshold 14") {
    StatInputs in = base_inputs();
    in.skills[6] = 15;
    // 35.667 + 5 = 40.667 -> 41
    CHECK(gl::game::compute_armor_weight(in).action_points == 41);
}

TEST_CASE("combat stats: action points drop by weight overload") {
    StatInputs in = base_inputs();
    in.carried.push_back(CarriedItem{1, 100000});
    CHECK(gl::game::compute_armor_weight(in).action_points == 5);
}

TEST_CASE("combat stats: action points clamp to 5..60") {
    StatInputs in = base_inputs();
    in.skills[26] = 1000;
    CHECK(gl::game::compute_armor_weight(in).action_points == 60);
}

TEST_CASE("combat stats: critical chance uses luck and skill 24") {
    StatInputs in = base_inputs();
    // (9*0.334 + 15*0.667) * 0.5 = 6.5055 -> 7
    CHECK(gl::game::compute_armor_weight(in).crit_chance == 7);
}

TEST_CASE("combat stats: critical chance is zero without skill 24") {
    StatInputs in = base_inputs();
    in.skills[24] = 0;
    CHECK(gl::game::compute_armor_weight(in).crit_chance == 0);
}

TEST_CASE("combat stats: critical chance applies percent modifiers and clamps to 10") {
    StatInputs in = base_inputs();
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x33, 50, 0x81));
    in.equipped.push_back(ring);
    CHECK(gl::game::compute_armor_weight(in).crit_chance == 10);
}

TEST_CASE("combat stats: physical damage uses the set A weapon damage pairs") {
    StatInputs in = base_inputs();
    in.equipped.push_back(axe_weapon());
    const ArmorWeight out = gl::game::compute_armor_weight(in);
    // blunt = damage[0..1]; slash = damage[2..3]; pierce = damage[4..5]
    CHECK(out.damage[0].minimum == 76);
    CHECK(out.damage[0].maximum == 96);
    CHECK(out.damage[2].minimum == 109);
    CHECK(out.damage[2].maximum == 123);
    CHECK(out.damage[1].minimum == 0);
    CHECK(out.damage[1].maximum == 0);
}

TEST_CASE("combat stats: elemental damage sums the weapon typed mods") {
    StatInputs in = base_inputs();
    in.equipped.push_back(axe_weapon());
    const ArmorWeight out = gl::game::compute_armor_weight(in);
    CHECK(out.damage[4].minimum == 1); // cold typed bonus 30
    CHECK(out.damage[4].maximum == 30);
    CHECK(out.damage[3].minimum == 0); // fire
    CHECK(out.damage[3].maximum == 0);
    CHECK(out.damage[5].minimum == 0); // poison
    CHECK(out.damage[5].maximum == 0);
}

TEST_CASE("combat stats: no weapon means zero damage") {
    StatInputs in = base_inputs();
    const ArmorWeight out = gl::game::compute_armor_weight(in);
    for (const auto &range : out.damage) {
        CHECK(range.minimum == 0);
        CHECK(range.maximum == 0);
    }
}

TEST_CASE("combat stats: accuracy with a weapon uses unknown_b[4] and weapon skill") {
    StatInputs in = base_inputs();
    in.equipped.push_back(axe_weapon());
    // (12*1.5 + 2*15 + 9/2)*0.334 + 50 = 67.368; (70+67.368)*0.6=82.42;
    // axe skill 13 > 9 -> +15 = 97.42 -> 97, clamped to 90
    CHECK(gl::game::compute_armor_weight(in).accuracy == 90);
}

TEST_CASE("combat stats: accuracy without a weapon uses skill 14 bonus") {
    StatInputs in = base_inputs();
    // (12*1.5 + 30 + 4)*0.334 + 50 = 67.368 -> 67
    CHECK(gl::game::compute_armor_weight(in).accuracy == 67);
    in.skills[14] = 15;
    CHECK(gl::game::compute_armor_weight(in).accuracy == 87);
}

TEST_CASE("combat stats: magic resistance uses wisdom and stat modifiers") {
    StatInputs in = base_inputs();
    CHECK(gl::game::compute_armor_weight(in).magic_resistance[2] == 10);
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x0d, 30, 0x41)); // stat 13 -> index 2
    in.equipped.push_back(ring);
    CHECK(gl::game::compute_armor_weight(in).magic_resistance[2] == 40);
}

TEST_CASE("combat stats: immunity uses perception and stat modifiers") {
    StatInputs in = base_inputs();
    CHECK(gl::game::compute_armor_weight(in).immunity[2] == 12);
    EquippedStatItem ring;
    ring.mods.push_back(mod(0x2a, 30, 0x41)); // stat 42 -> index 2
    in.equipped.push_back(ring);
    CHECK(gl::game::compute_armor_weight(in).immunity[2] == 42);
}

TEST_CASE("combat stats: elemental resistance uses its skill and skill 20 bonus") {
    StatInputs in = base_inputs();
    const ArmorWeight out = gl::game::compute_armor_weight(in);
    // fire = skills[22]=1 +10 (skills[20]>4); cold = skills[25]=0 +10; poison = skills[20]=14 +10
    CHECK(out.elemental_resistance[0] == 11);
    CHECK(out.elemental_resistance[1] == 10);
    CHECK(out.elemental_resistance[2] == 24);
}

TEST_CASE("combat stats: physical resistance sums armor pieces per damage index") {
    StatInputs in = base_inputs();
    in.equipped.push_back(armor_piece(0x100000u, {10, 0, 20, 0, 30, 0}));
    const ArmorWeight out = gl::game::compute_armor_weight(in);
    // blunt dmg[0]=10*0.334 + (skill6 10 + con 25*0.334) -> 22
    // pierce dmg[4]=30*0.334 + (skill6 10 + dex 18*0.334) -> 26
    // slash dmg[2]=20*0.333 + (skill6 10 + per 12*0.334) -> 21
    CHECK(out.physical_resistance[0] == 22);
    CHECK(out.physical_resistance[1] == 26);
    CHECK(out.physical_resistance[2] == 21);
}

TEST_CASE("combat stats: primary attributes apply equipment modifiers") {
    StatInputs in = base_inputs();
    const ArmorWeight base = gl::game::compute_armor_weight(in);
    const std::array<std::int32_t, 7> expected{30, 25, 18, 12, 10, 5, 9};
    CHECK(base.primary == expected);
    EquippedStatItem ring;
    ring.mods.push_back(mod(9, 2, 0x41)); // +2 dexterity
    in.equipped.push_back(ring);
    CHECK(gl::game::compute_armor_weight(in).primary[2] == 20);
}

TEST_CASE("combat stats: format helpers") {
    CHECK(gl::game::format_armor(34) == "34");
    CHECK(gl::game::format_weight(1320, 1710) == "132/171");
    CHECK(gl::game::format_weight(5, 2046) == "0.5/204.6");
    CHECK(gl::game::format_damage(gl::game::DamageRange{76, 96}) == "76-96");
    CHECK(gl::game::format_damage(gl::game::DamageRange{0, 0}) == "0");
    CHECK(gl::game::format_damage(gl::game::DamageRange{1, 30}) == "1-30");
}

TEST_CASE("character sheet fills armor, weight and action points when provided") {
    gl::assets::HeroStats stats;
    stats.strength = 30;
    stats.constitution = 25;
    stats.dexterity = 18;
    stats.experience = 81099;
    const auto runtime = gl::game::PersonRuntime::create({}, stats);
    ArmorWeight values;
    values.armor_class = 15;
    values.weight_cur_x10 = 1320;
    values.weight_max_x10 = 2046;
    values.action_points = 43;
    values.crit_chance = 7;
    values.accuracy = 90;
    values.damage = {{{76, 96}, {0, 0}, {125, 141}, {0, 0}, {1, 30}, {0, 0}}};
    values.primary = {{30, 30, 20, 13, 10, 5, 9}};
    values.magic_resistance = {{40, 40, 40, 40, 10, 10}};
    values.physical_resistance = {{64, 71, 79}};
    values.elemental_resistance = {{11, 10, 24}};
    values.immunity = {{13, 38, 38, 13, 38, 13}};
    const auto sheet = gl::game::build_character_sheet(
        runtime, [](std::uint32_t id) { return "L" + std::to_string(id); }, "Вертас", "Hero",
        &values);
    REQUIRE(sheet.primary.size() == 7);
    CHECK(sheet.primary[0].value == "30");
    CHECK(sheet.primary[1].value == "30");
    CHECK(sheet.primary[2].value == "20");
    REQUIRE(sheet.combat.size() == 10);
    CHECK(sheet.combat[0].value == "76-96");
    CHECK(sheet.combat[1].value == "0");
    CHECK(sheet.combat[2].value == "125-141");
    CHECK(sheet.combat[3].value == "0");
    CHECK(sheet.combat[4].value == "1-30");
    CHECK(sheet.combat[5].value == "0");
    for (std::size_t index = 0; index < 6; ++index)
        CHECK(sheet.combat[index].modeled);
    CHECK(sheet.combat[6].value == "90%");
    CHECK(sheet.combat[6].modeled);
    CHECK(sheet.combat[7].label == "L26");
    CHECK(sheet.combat[7].value == "7%");
    CHECK(sheet.combat[7].modeled);
    CHECK(sheet.combat[8].label == "L17");
    CHECK(sheet.combat[8].value == "15");
    CHECK(sheet.combat[8].modeled);
    CHECK(sheet.combat[9].label == "L29");
    CHECK(sheet.combat[9].value == "132/204.6");
    CHECK(sheet.combat[9].modeled);
    REQUIRE(sheet.state.size() == 6);
    CHECK(sheet.state[5].label == "L18");
    CHECK(sheet.state[5].value == "43");
    CHECK(sheet.state[5].modeled);
    REQUIRE(sheet.resistance.size() == 12);
    CHECK(sheet.resistance[3].value == "40%");
    CHECK(sheet.resistance[3].modeled);
    CHECK(sheet.resistance[7].value == "10%");
    CHECK(sheet.resistance[9].value == "11%");
    CHECK(sheet.resistance[11].value == "24%");
    CHECK(sheet.resistance[0].value == "64%");
    CHECK(sheet.resistance[1].value == "71%");
    CHECK(sheet.resistance[2].value == "79%");
    CHECK(sheet.resistance[0].modeled);
    REQUIRE(sheet.immunity.size() == 6);
    CHECK(sheet.immunity[0].value == "13%");
    CHECK(sheet.immunity[1].value == "38%");
    CHECK(sheet.immunity[0].modeled);
}

TEST_CASE("combat stats real: slot0 armor and weight" * doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto catalog = gl::assets::ItemCatalog::load(vfs.value());
    REQUIRE(catalog.has_value());

    const std::filesystem::path path = std::filesystem::path(dir) / "Saves" / "slot0.sav";
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    auto body = gl::assets::decode_save_body(bytes);
    REQUIRE(body.has_value());
    auto stats = gl::assets::parse_hero_stats(body.value());
    REQUIRE(stats.has_value());
    auto inventory = gl::assets::parse_hero_inventory(body.value());
    REQUIRE(inventory.has_value());

    StatInputs inputs;
    inputs.strength = stats->strength;
    inputs.constitution = stats->constitution;
    inputs.dexterity = stats->dexterity;
    inputs.perception = stats->perception;
    inputs.luck = stats->luck;
    inputs.wisdom = stats->wisdom;
    inputs.intelligence = stats->intelligence;
    inputs.experience = stats->experience;
    inputs.person_flags = stats->flags;
    for (std::size_t id = 0; id < inputs.skills.size(); ++id)
        inputs.skills[id] = stats->skills[id];

    for (const auto &item : inventory->equipped) {
        EquippedStatItem equipped;
        equipped.type = item.type;
        equipped.flags = item.flags;
        equipped.durability = item.durability;
        equipped.mods = item.mods;
        if (const auto *entry = catalog->find(item.item_id)) {
            equipped.unknown_b = entry->item.unknown_b;
            equipped.damage = entry->item.damage;
        }
        inputs.equipped.push_back(equipped);
    }
    for (const auto &list : {inventory->hot, inventory->items, inventory->equipped})
        for (const auto &item : list)
            inputs.carried.push_back(
                CarriedItem{static_cast<std::int32_t>(item.count), item.weight_x10});

    const ArmorWeight out = gl::game::compute_armor_weight(inputs);
    CHECK(out.weight_cur_x10 == 1320);
    CHECK(out.weight_max_x10 == 2046);
    CHECK(out.armor_class == 15);
    CHECK(out.action_points == 43);
    CHECK(out.crit_chance == 7);
    CHECK(out.accuracy == 90);
    // set A weapon = axe 486 (damage 47,62,72,83,0,0; cold mod +30)
    CHECK(out.damage[0].minimum == 76);
    CHECK(out.damage[0].maximum == 96);
    CHECK(out.damage[1].minimum == 0);
    CHECK(out.damage[1].maximum == 0);
    CHECK(out.damage[2].minimum == 125);
    CHECK(out.damage[2].maximum == 141);
    CHECK(out.damage[3].minimum == 0);
    CHECK(out.damage[3].maximum == 0);
    CHECK(out.damage[4].minimum == 1);
    CHECK(out.damage[4].maximum == 30);
    CHECK(out.damage[5].minimum == 0);
    CHECK(out.damage[5].maximum == 0);
    const std::array<std::int32_t, 6> magic{40, 40, 40, 40, 10, 10};
    const std::array<std::int32_t, 6> immunity{13, 38, 38, 13, 38, 13};
    const std::array<std::int32_t, 3> elemental{11, 10, 24};
    const std::array<std::int32_t, 3> physical{64, 71, 79};
    const std::array<std::int32_t, 7> primary{30, 30, 20, 13, 10, 5, 9};
    CHECK(out.primary == primary);
    CHECK(out.magic_resistance == magic);
    CHECK(out.immunity == immunity);
    CHECK(out.elemental_resistance == elemental);
    CHECK(out.physical_resistance == physical);
}
