#include "gl/game/character_sheet.hpp"
#include "gl/script/person_access.hpp"
#include <array>
#include <string>
#include <utility>

namespace gl::game {
namespace {
using gl::script::PersonParameter;

constexpr std::array<std::uint32_t, 8> kPrimaryIds{6, 7, 8, 9, 10, 11, 12, 16};
constexpr std::array<PersonParameter, 8> kPrimaryParams{
    PersonParameter::strength,     PersonParameter::constitution,
    PersonParameter::dexterity,    PersonParameter::perception,
    PersonParameter::wisdom,       PersonParameter::intelligence,
    PersonParameter::luck,         PersonParameter::reputation};
constexpr std::array<std::uint32_t, 15> kCharacteristicIds{
    14, 15, 16, 27, 28, 18, 19, 20, 21, 22, 23, 24, 25, 26, 17};
constexpr std::array<std::uint32_t, 27> kSkillUiIds{
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
    63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75};
constexpr std::array<std::string_view, 27> kSkillNames{
    "SKILL_WPN_SWORD", "SKILL_WPN_AXE",   "SKILL_WPN_CRUSH", "SKILL_WPN_STAFF",
    "SKILL_WPN_DIST",  "SKILL_WPN_SPEAR", "SKILL_WPN_THROW", "SKILL_WPN_HAND",
    "SKILL_CRITICAL_HIT", "SKILL_SHADMAG", "SKILL_NATRMAG",  "SKILL_GODSMAG",
    "SKILL_ELEMMAG",   "SKILL_LGHTMAG",   "SKILL_DARKMAG",  "SKILL_MAGICUSE",
    "SKILL_ALCHEMY",   "SKILL_IDENTIFY",  "SKILL_TACTIC",   "SKILL_SCOUT",
    "SKILL_HEALING",   "SKILL_TALKING",   "SKILL_TRADE",    "SKILL_STEAL",
    "SKILL_HACK",      "SKILL_SMITH",     "SKILL_ATHLETIC"};

std::string label_of(const std::function<std::string(std::uint32_t)> &ui_text,
                     std::uint32_t id) {
    std::string text = ui_text(id);
    return text.empty() ? "#" + std::to_string(id) : text;
}

std::string integer_value(double value) {
    return std::to_string(static_cast<std::int32_t>(value));
}

CharacterRow row(const std::function<std::string(std::uint32_t)> &ui_text,
                 std::uint32_t id, std::string value, bool modeled = true) {
    return CharacterRow{label_of(ui_text, id), std::move(value), modeled};
}
} // namespace

CharacterSheet build_character_sheet(
    const PersonRuntime &persons,
    const std::function<std::string(std::uint32_t)> &ui_text,
    std::string_view person) {
    CharacterSheet sheet;
    sheet.name = std::string(person);
    sheet.skill_points = 0;
    for (std::size_t index = 0; index < kPrimaryIds.size(); ++index) {
        sheet.primary.push_back(row(ui_text, kPrimaryIds[index],
                                    integer_value(persons.get_parameter(person,
                                                                         kPrimaryParams[index]))));
    }

    const double experience =
        persons.get_parameter(person, PersonParameter::experience);
    const double reputation =
        persons.get_parameter(person, PersonParameter::reputation);
    sheet.personality.push_back(row(ui_text, 5, sheet.name));
    sheet.personality.push_back(row(
        ui_text, 14,
        integer_value(static_cast<double>(hero_level_from_experience(experience)))));
    sheet.personality.push_back(row(ui_text, 15, integer_value(experience)));
    sheet.personality.push_back(row(ui_text, 16, integer_value(reputation)));

    const std::array<std::string, 15> characteristic_values{
        integer_value(static_cast<double>(hero_level_from_experience(experience))),
        integer_value(experience),
        integer_value(reputation),
        integer_value(persons.get_parameter(person, PersonParameter::life)),
        integer_value(persons.get_parameter(person, PersonParameter::energy)),
        "-", "-", "-", "-", "-", "-", "-", "-", "-", "-"};
    const std::array<bool, 15> characteristic_modeled{
        true, true, true, true, true, false, false, false, false, false,
        false, false, false, false, false};
    for (std::size_t index = 0; index < kCharacteristicIds.size(); ++index) {
        sheet.characteristics.push_back(row(ui_text, kCharacteristicIds[index],
                                            characteristic_values[index],
                                            characteristic_modeled[index]));
    }

    for (std::size_t index = 0; index < kSkillUiIds.size(); ++index) {
        sheet.skills.push_back(row(
            ui_text, kSkillUiIds[index],
            integer_value(persons.get_skill(person, kSkillNames[index]))));
    }
    return sheet;
}

CharacterSheet build_character_sheet(const PersonRuntime &persons,
                                     const gl::assets::Sdb &ui_texts,
                                     std::string_view person) {
    return build_character_sheet(
        persons,
        [&ui_texts](std::uint32_t id) {
            const std::string *text = ui_texts.find(id);
            return text == nullptr ? std::string{} : *text;
        },
        person);
}
} // namespace gl::game
