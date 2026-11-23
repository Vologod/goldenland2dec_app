#pragma once
#include "gl/assets/sdb.hpp"
#include "gl/game/person_runtime.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
enum class CharacterTab { personality, characteristics, skills, state, resistance };

struct CharacterRow {
    std::string label;
    std::string value;
    bool modeled = true;
};

struct CharacterSheet {
    std::string name;
    std::vector<CharacterRow> primary;
    std::vector<CharacterRow> personality;
    std::vector<CharacterRow> characteristics;
    std::vector<CharacterRow> skills;
    std::int32_t skill_points = 0;
};

CharacterSheet build_character_sheet(
    const PersonRuntime &persons,
    const std::function<std::string(std::uint32_t)> &ui_text,
    std::string_view person = "Hero");
CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const gl::assets::Sdb &ui_texts,
                                     std::string_view person = "Hero");
} // namespace gl::game
