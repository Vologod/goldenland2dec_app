#pragma once
#include "gl/assets/sdb.hpp"
#include "gl/game/combat_stats.hpp"
#include "gl/game/person_runtime.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
enum class CharacterTab { characteristics, skills };

struct CharacterRow {
    std::string label;
    std::string value;
    bool modeled = true;
};

struct CharacterGroup {
    std::string title;
    std::vector<CharacterRow> rows;
};

struct CharacterSheet {
    std::string name;
    std::string characteristics_title;
    std::string skills_title;
    std::string state_title;
    std::string combat_title;
    std::string resistance_title;
    std::string immunity_title;
    std::string skill_points_label;
    std::vector<CharacterRow> primary;
    CharacterRow experience_points;
    std::vector<CharacterRow> state;
    std::vector<CharacterRow> combat;
    std::vector<CharacterRow> resistance;
    std::vector<CharacterRow> immunity;
    std::vector<CharacterGroup> skill_groups;
    std::int32_t skill_points = 0;
    std::uint32_t experience_max = 0;
};

CharacterSheet build_character_sheet(
    const PersonRuntime &persons,
    const std::function<std::string(std::uint32_t)> &ui_text,
    std::string_view display_name, std::string_view person = "Hero");
CharacterSheet build_character_sheet(
    const PersonRuntime &persons,
    const std::function<std::string(std::uint32_t)> &ui_text,
    std::string_view display_name, std::string_view person,
    const ArmorWeight *armor_weight);
CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const gl::assets::Sdb &ui_texts,
                                     std::string_view display_name,
                                     std::string_view person = "Hero");
CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const gl::assets::Sdb &ui_texts,
                                     std::string_view display_name,
                                     std::string_view person,
                                     const ArmorWeight *armor_weight);
} // namespace gl::game
