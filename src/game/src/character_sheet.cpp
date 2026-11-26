#include "gl/game/character_sheet.hpp"
#include "gl/script/person_access.hpp"
#include <array>
#include <string>
#include <utility>

namespace gl::game {
namespace {
using gl::script::PersonParameter;

constexpr std::array<std::uint32_t, 7> kPrimaryIds{6, 7, 8, 9, 10, 11, 12};
constexpr std::array<PersonParameter, 7> kPrimaryParams{
    PersonParameter::strength,   PersonParameter::constitution, PersonParameter::dexterity,
    PersonParameter::perception, PersonParameter::wisdom,       PersonParameter::intelligence,
    PersonParameter::luck};
constexpr std::array<std::uint32_t, 6> kStateIds{14, 15, 16, 27, 28, 18};
constexpr std::array<std::uint32_t, 10> kCombatIds{19, 20, 21, 22, 23, 24, 25, 26, 17, 29};
constexpr std::array<std::uint32_t, 12> kResistanceIds{30, 31, 32, 33, 34, 35,
                                                       36, 37, 38, 39, 40, 41};
constexpr std::array<std::uint32_t, 6> kImmunityIds{43, 44, 45, 46, 47, 48};
constexpr std::array<std::uint32_t, 27> kSkillUiIds{49, 50, 51, 52, 53, 54, 55, 56, 57,
                                                    58, 59, 60, 61, 62, 63, 64, 65, 66,
                                                    67, 68, 69, 70, 71, 72, 73, 74, 75};
constexpr std::array<std::string_view, 27> kSkillNames{
    "SKILL_WPN_SWORD", "SKILL_WPN_AXE",   "SKILL_WPN_CRUSH", "SKILL_WPN_STAFF",    "SKILL_WPN_DIST",
    "SKILL_WPN_SPEAR", "SKILL_WPN_THROW", "SKILL_WPN_HAND",  "SKILL_CRITICAL_HIT", "SKILL_SHADMAG",
    "SKILL_NATRMAG",   "SKILL_GODSMAG",   "SKILL_ELEMMAG",   "SKILL_LGHTMAG",      "SKILL_DARKMAG",
    "SKILL_MAGICUSE",  "SKILL_ALCHEMY",   "SKILL_IDENTIFY",  "SKILL_TACTIC",       "SKILL_SCOUT",
    "SKILL_HEALING",   "SKILL_TALKING",   "SKILL_TRADE",     "SKILL_STEAL",        "SKILL_HACK",
    "SKILL_SMITH",     "SKILL_ATHLETIC"};
constexpr std::array<std::uint32_t, 3> kSkillGroupTitles{83, 84, 85};

std::string label_of(const std::function<std::string(std::uint32_t)> &ui_text, std::uint32_t id) {
    std::string text = ui_text(id);
    return text.empty() ? "#" + std::to_string(id) : text;
}

std::string integer_value(double value) { return std::to_string(static_cast<std::int32_t>(value)); }

CharacterRow row(const std::function<std::string(std::uint32_t)> &ui_text, std::uint32_t id,
                 std::string value, bool modeled = true) {
    return CharacterRow{label_of(ui_text, id), std::move(value), modeled};
}

std::uint32_t next_level_experience(int level) {
    if (level < 1)
        level = 1;
    return static_cast<std::uint32_t>(50 * level * (level + 1));
}
} // namespace

CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const std::function<std::string(std::uint32_t)> &ui_text,
                                     std::string_view display_name, std::string_view person) {
    return build_character_sheet(persons, ui_text, display_name, person, nullptr);
}

CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const std::function<std::string(std::uint32_t)> &ui_text,
                                     std::string_view display_name, std::string_view person,
                                     const ArmorWeight *armor_weight) {
    CharacterSheet sheet;
    sheet.name = display_name.empty() ? std::string(person) : std::string(display_name);
    sheet.characteristics_title = label_of(ui_text, 1);
    sheet.skills_title = label_of(ui_text, 2);
    sheet.state_title = label_of(ui_text, 3);
    sheet.combat_title = label_of(ui_text, 86);
    sheet.resistance_title = label_of(ui_text, 4);
    sheet.immunity_title = label_of(ui_text, 42);
    sheet.skill_points_label = label_of(ui_text, 76);
    sheet.skill_points = 0;
    for (std::size_t index = 0; index < kPrimaryIds.size(); ++index) {
        sheet.primary.push_back(
            row(ui_text, kPrimaryIds[index],
                integer_value(persons.get_parameter(person, kPrimaryParams[index]))));
    }
    sheet.experience_points = row(ui_text, 13, "0");

    const double experience = persons.get_parameter(person, PersonParameter::experience);
    const double reputation = persons.get_parameter(person, PersonParameter::reputation);
    const int level = hero_level_from_experience(experience);
    const std::uint32_t maximum = next_level_experience(level);
    sheet.experience_max = maximum;
    const double life = persons.get_parameter(person, PersonParameter::life);
    const double maximum_life = persons.get_parameter(person, PersonParameter::maximum_life);
    const double energy = persons.get_parameter(person, PersonParameter::energy);
    const double maximum_energy = persons.maximum_energy(person);

    const std::array<std::string, 6> state_values{
        integer_value(static_cast<double>(level)),
        integer_value(experience) + "/" + std::to_string(maximum),
        integer_value(reputation),
        integer_value(life) + "/" + integer_value(maximum_life),
        integer_value(energy) + "/" + integer_value(maximum_energy),
        "—"};
    const std::array<bool, 6> state_modeled{true, true, true, true, true, false};
    for (std::size_t index = 0; index < kStateIds.size(); ++index) {
        sheet.state.push_back(
            row(ui_text, kStateIds[index], state_values[index], state_modeled[index]));
    }
    if (armor_weight != nullptr) {
        for (std::size_t index = 0; index < armor_weight->primary.size(); ++index)
            sheet.primary[index].value = std::to_string(armor_weight->primary[index]);
        sheet.state[5] = row(ui_text, kStateIds[5], format_armor(armor_weight->action_points));
    }
    for (const std::uint32_t id : kCombatIds)
        sheet.combat.push_back(row(ui_text, id, "—", false));
    if (armor_weight != nullptr) {
        for (std::size_t index = 0; index < armor_weight->damage.size(); ++index)
            sheet.combat[index] =
                row(ui_text, kCombatIds[index], format_damage(armor_weight->damage[index]));
        sheet.combat[6] = row(ui_text, kCombatIds[6], std::to_string(armor_weight->accuracy) + "%");
        sheet.combat[7] =
            row(ui_text, kCombatIds[7], std::to_string(armor_weight->crit_chance) + "%");
        sheet.combat[8] = row(ui_text, kCombatIds[8], format_armor(armor_weight->armor_class));
        sheet.combat[9] =
            row(ui_text, kCombatIds[9],
                format_weight(armor_weight->weight_cur_x10, armor_weight->weight_max_x10));
    }
    for (const std::uint32_t id : kResistanceIds)
        sheet.resistance.push_back(row(ui_text, id, "—", false));
    for (const std::uint32_t id : kImmunityIds)
        sheet.immunity.push_back(row(ui_text, id, "—", false));
    if (armor_weight != nullptr) {
        for (std::size_t index = 0; index < armor_weight->physical_resistance.size(); ++index)
            sheet.resistance[index] =
                row(ui_text, kResistanceIds[index],
                    std::to_string(armor_weight->physical_resistance[index]) + "%");
        for (std::size_t index = 0; index < armor_weight->magic_resistance.size(); ++index)
            sheet.resistance[3 + index] =
                row(ui_text, kResistanceIds[3 + index],
                    std::to_string(armor_weight->magic_resistance[index]) + "%");
        for (std::size_t index = 0; index < armor_weight->elemental_resistance.size(); ++index)
            sheet.resistance[9 + index] =
                row(ui_text, kResistanceIds[9 + index],
                    std::to_string(armor_weight->elemental_resistance[index]) + "%");
        for (std::size_t index = 0; index < armor_weight->immunity.size(); ++index)
            sheet.immunity[index] = row(ui_text, kImmunityIds[index],
                                        std::to_string(armor_weight->immunity[index]) + "%");
    }

    for (std::size_t group = 0; group < kSkillGroupTitles.size(); ++group) {
        CharacterGroup entry;
        entry.title = label_of(ui_text, kSkillGroupTitles[group]);
        for (std::size_t offset = 0; offset < 9; ++offset) {
            const std::size_t index = group * 9 + offset;
            entry.rows.push_back(row(ui_text, kSkillUiIds[index],
                                     integer_value(persons.get_skill(person, kSkillNames[index]))));
        }
        sheet.skill_groups.push_back(std::move(entry));
    }
    return sheet;
}

CharacterSheet build_character_sheet(const PersonRuntime &persons, const gl::assets::Sdb &ui_texts,
                                     std::string_view display_name, std::string_view person) {
    return build_character_sheet(persons, ui_texts, display_name, person, nullptr);
}

CharacterSheet build_character_sheet(const PersonRuntime &persons, const gl::assets::Sdb &ui_texts,
                                     std::string_view display_name, std::string_view person,
                                     const ArmorWeight *armor_weight) {
    return build_character_sheet(
        persons,
        [&ui_texts](std::uint32_t id) {
            const std::string *text = ui_texts.find(id);
            return text == nullptr ? std::string{} : *text;
        },
        display_name, person, armor_weight);
}
} // namespace gl::game
