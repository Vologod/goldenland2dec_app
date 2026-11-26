#pragma once
#include "gl/assets/hero_equipment.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gl::game {
struct EquippedStatItem {
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t durability = 100;
    std::array<std::uint32_t, 5> unknown_b{};
    std::array<std::uint32_t, 6> damage{};
    std::vector<gl::assets::SpecialMod> mods;
};

struct CarriedItem {
    std::int32_t count = 0;
    std::uint32_t weight_x10 = 0;
};

// Current world lighting for conditional (day/night) stat modifiers.
// Unknown means conditional modifiers are skipped (legacy behaviour).
struct StatCondition {
    bool day_night_known = false;
    double day_night = 0.0;
};

struct StatInputs {
    std::int32_t strength = 0;
    std::int32_t constitution = 0;
    std::int32_t dexterity = 0;
    std::int32_t perception = 0;
    std::int32_t luck = 0;
    std::int32_t wisdom = 0;
    std::int32_t intelligence = 0;
    std::int32_t experience = 0;
    std::uint32_t person_flags = 0;
    StatCondition condition;
    std::array<std::int32_t, 27> skills{};
    std::vector<EquippedStatItem> equipped;
    std::vector<gl::assets::SpecialMod> effect_mods;
    std::vector<CarriedItem> carried;
};

struct DamageRange {
    std::int32_t minimum = 0;
    std::int32_t maximum = 0;
};

struct ArmorWeight {
    std::int32_t armor_class = 0;
    std::int32_t weight_cur_x10 = 0;
    std::int32_t weight_max_x10 = 0;
    std::int32_t action_points = 0;
    std::int32_t crit_chance = 0;
    std::int32_t accuracy = 0;
    // UI primary order: strength, constitution, dexterity, perception, wisdom, intelligence, luck.
    std::array<std::int32_t, 7> primary{};
    // Resistance rows 0..2 (UI ids 30..32): blunt/pierce/slash.
    std::array<std::int32_t, 3> physical_resistance{};
    // Resistance rows 3..8 (UI ids 33..38): shadow/nature/gods/elements/light/dark.
    std::array<std::int32_t, 6> magic_resistance{};
    // Resistance rows 9..11 (UI ids 39..41): fire/cold/poison.
    std::array<std::int32_t, 3> elemental_resistance{};
    // Immunity rows (UI ids 43..48): shadow/nature/gods/elements/light/dark.
    std::array<std::int32_t, 6> immunity{};
    // Order matches the combat rows: blunt, pierce, slash, fire, cold, poison.
    std::array<DamageRange, 6> damage{};
};

ArmorWeight compute_armor_weight(const StatInputs &inputs);
std::string format_armor(std::int32_t armor_class);
std::string format_weight(std::int32_t cur_x10, std::int32_t max_x10);
std::string format_damage(const DamageRange &damage);
} // namespace gl::game
