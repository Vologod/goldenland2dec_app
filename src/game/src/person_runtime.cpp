#include "gl/game/person_runtime.hpp"

#include "gl/assets/log.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace gl::game {
namespace {
constexpr double kMaxPrimary = 30.0;
constexpr double kMaxSkill = 15.0;
constexpr double kReputationMinimum = -50.0;
constexpr double kReputationMaximum = 50.0;
constexpr double kMaxExperience = 500000.0;

constexpr std::array<std::string_view, 27> kHeroSkillNames{
    "SKILL_GODSMAG", "SKILL_ELEMMAG",  "SKILL_LGHTMAG", "SKILL_DARKMAG", "SKILL_SHADMAG",
    "SKILL_NATRMAG", "SKILL_ATHLETIC", "SKILL_WPN_CRUSH", "SKILL_WPN_STAFF", "SKILL_WPN_DIST",
    "SKILL_WPN_SPEAR", "SKILL_WPN_THROW", "SKILL_WPN_SWORD", "SKILL_WPN_AXE", "SKILL_WPN_HAND",
    "SKILL_SCOUT",   "SKILL_TRADE",    "SKILL_MAGICUSE", "SKILL_STEAL", "SKILL_HACK",
    "SKILL_HEALING", "SKILL_TALKING",  "SKILL_SMITH",   "SKILL_IDENTIFY", "SKILL_CRITICAL_HIT",
    "SKILL_ALCHEMY", "SKILL_TACTIC"};

using gl::script::PersonParameter;

std::size_t value_index(PersonParameter parameter) { return static_cast<std::size_t>(parameter); }

double clamp_value(double value, double minimum, double maximum) {
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

std::int32_t round_half_away_from_zero(double value) {
    if (!std::isfinite(value))
        return 0;
    const double adjusted = value + (value > 0.0 ? 0.5 : -0.5);
    const double truncated = std::trunc(adjusted);
    if (truncated >= static_cast<double>(std::numeric_limits<std::int32_t>::max()))
        return std::numeric_limits<std::int32_t>::max();
    if (truncated <= static_cast<double>(std::numeric_limits<std::int32_t>::min()))
        return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(truncated);
}

double parse_number(std::string_view text) {
    if (text.empty())
        return 0.0;
    double value = 0.0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        return 0.0;
    return value;
}

void assign_value(PersonState &state, PersonParameter parameter,
                  const std::map<std::string, std::string> &values, std::string_view key) {
    const auto it = values.find(std::string(key));
    if (it == values.end())
        return;
    const std::size_t index = value_index(parameter);
    state.values[index] = parse_number(it->second);
    state.present[index] = true;
}

double skill_value(const PersonState &state, std::string_view skill) {
    const auto it = state.skills.find(skill);
    return it == state.skills.end() ? 0.0 : it->second;
}

double strength_value(const PersonState &state) {
    const double base = static_cast<double>(
        round_half_away_from_zero(state.values[value_index(PersonParameter::strength)]));
    const double bonus = state.modifier_gate ? state.strength_bonus : 0.0;
    return clamp_value(base + bonus, 1.0, kMaxPrimary);
}

double constitution_value(const PersonState &state) {
    const double base = static_cast<double>(
        round_half_away_from_zero(state.values[value_index(PersonParameter::constitution)]));
    return clamp_value(base, 1.0, kMaxPrimary);
}

double dexterity_value(const PersonState &state) {
    const double base = static_cast<double>(
        round_half_away_from_zero(state.values[value_index(PersonParameter::dexterity)]));
    return clamp_value(base, 1.0, kMaxPrimary);
}

double perception_value(const PersonState &state) {
    const double base = static_cast<double>(
        round_half_away_from_zero(state.values[value_index(PersonParameter::perception)]));
    return clamp_value(base, 1.0, kMaxPrimary);
}

double intelligence_value(const PersonState &state) {
    const double base = static_cast<double>(
        round_half_away_from_zero(state.values[value_index(PersonParameter::intelligence)]));
    const double talking = skill_value(state, "SKILL_TALKING") > 9.0 ? 2.0 : 0.0;
    return clamp_value(base + talking, 1.0, kMaxPrimary);
}

double wisdom_value(const PersonState &state) {
    const double base = static_cast<double>(
        round_half_away_from_zero(state.values[value_index(PersonParameter::wisdom)]));
    const double talking = skill_value(state, "SKILL_TALKING") > 4.0 ? 1.0 : 0.0;
    return clamp_value(base + talking, 1.0, kMaxPrimary);
}

double luck_value(const PersonState &state) {
    const double base = static_cast<double>(
        round_half_away_from_zero(state.values[value_index(PersonParameter::luck)]));
    return clamp_value(base, 1.0, kMaxPrimary);
}

double reputation_value(const PersonState &state) {
    const double stored = clamp_value(state.values[value_index(PersonParameter::reputation)],
                                      kReputationMinimum, kReputationMaximum);
    const double rounded = static_cast<double>(round_half_away_from_zero(stored));
    return clamp_value(rounded, kReputationMinimum, kReputationMaximum);
}

double experience_value(const PersonState &state) {
    return clamp_value(state.values[value_index(PersonParameter::experience)], 0.0, kMaxExperience);
}

int level_from_experience(double experience) {
    int level = 1;
    if (experience > 99.0) {
        int step = 50;
        do {
            step += 50;
            ++level;
        } while (static_cast<double>(level) * step + step <= experience);
    }
    if (level > 98)
        level = 99;
    return level;
}

double maximum_life_value(const PersonState &state) {
    const int level = level_from_experience(experience_value(state));
    const int strength = static_cast<int>(strength_value(state));
    const int constitution = static_cast<int>(constitution_value(state));
    const float multiplier = static_cast<float>(level) * 0.02f + 1.0f;
    const float raw = static_cast<float>((strength >> 1) * 3 + constitution * 3) * multiplier;
    int result = round_half_away_from_zero(static_cast<double>(raw));
    if (result < 2)
        result = 1;
    else if (result > 998)
        result = 999;
    return static_cast<double>(result);
}

double maximum_energy_value(const PersonState &state) {
    const int level = level_from_experience(experience_value(state));
    const int intelligence = static_cast<int>(intelligence_value(state));
    const int wisdom = static_cast<int>(wisdom_value(state));
    const float multiplier = static_cast<float>(level) * 0.01f + 1.0f;
    const float raw = static_cast<float>((intelligence >> 1) * 3 + wisdom * 3) * multiplier;
    int result = round_half_away_from_zero(static_cast<double>(raw));
    if (result < 1)
        result = 0;
    else if (result > 998)
        result = 999;
    return static_cast<double>(result);
}

double life_value(const PersonState &state) {
    return clamp_value(state.values[value_index(PersonParameter::life)], 0.0,
                       maximum_life_value(state));
}

double energy_value(const PersonState &state) {
    return clamp_value(state.values[value_index(PersonParameter::energy)], 0.0,
                       maximum_energy_value(state));
}

void set_primary(PersonState &state, PersonParameter parameter, double value) {
    if (value < 1.0 || value > kMaxPrimary)
        return;
    const double own_bonus = parameter == PersonParameter::strength ? state.strength_bonus : 0.0;
    const double base = state.values[value_index(parameter)];
    if (value == base + own_bonus)
        return;
    const double delta = value - (base + own_bonus);
    state.strength_bonus += delta;
}

void set_reputation(PersonState &state, double value) {
    const double target = clamp_value(value, kReputationMinimum, kReputationMaximum);
    const double current = reputation_value(state);
    double delta = target - current;
    if (delta > 0.0) {
        if (skill_value(state, "SKILL_LGHTMAG") > 4.0)
            delta *= 2.0;
    } else if (delta < 0.0) {
        if (skill_value(state, "SKILL_DARKMAG") > 4.0)
            delta *= 2.0;
    }
    state.values[value_index(PersonParameter::reputation)] += delta;
}
} // namespace

int hero_level_from_experience(double experience) { return level_from_experience(experience); }

PersonRuntime PersonRuntime::create(const std::vector<PersonInstance> &persons,
                                    const std::optional<gl::assets::HeroStats> &hero) {
    PersonRuntime runtime;
    runtime.persons_.reserve(persons.size() + 1);

    const auto add = [&runtime](PersonState state) {
        const auto [it, inserted] = runtime.index_.emplace(state.name, runtime.persons_.size());
        if (!inserted) {
            gl::log(gl::LogLevel::warning,
                    "person runtime: duplicate person name '" + state.name + "' ignored");
            return;
        }
        runtime.persons_.push_back(std::move(state));
    };

    PersonState hero_state;
    hero_state.name = "Hero";
    if (hero) {
        hero_state.present.fill(true);
        hero_state.values[value_index(PersonParameter::experience)] = hero->experience;
        hero_state.values[value_index(PersonParameter::reputation)] = hero->reputation;
        hero_state.values[value_index(PersonParameter::intelligence)] = hero->intelligence;
        hero_state.values[value_index(PersonParameter::wisdom)] = hero->wisdom;
        hero_state.values[value_index(PersonParameter::strength)] = hero->strength;
        hero_state.values[value_index(PersonParameter::constitution)] = hero->constitution;
        hero_state.values[value_index(PersonParameter::dexterity)] = hero->dexterity;
        hero_state.values[value_index(PersonParameter::luck)] = hero->luck;
        hero_state.values[value_index(PersonParameter::perception)] = hero->perception;
        hero_state.values[value_index(PersonParameter::life)] = hero->life;
        hero_state.values[value_index(PersonParameter::energy)] = hero->energy;
        for (std::size_t id = 0; id < kHeroSkillNames.size(); ++id)
            hero_state.skills.emplace(kHeroSkillNames[id], hero->skills[id]);
    }
    add(std::move(hero_state));

    for (const PersonInstance &person : persons) {
        PersonState state;
        state.name = person.name;
        assign_value(state, PersonParameter::experience, person.def.values, "experience_value");
        assign_value(state, PersonParameter::reputation, person.def.values, "reputation");
        assign_value(state, PersonParameter::intelligence, person.def.values, "intelligence");
        assign_value(state, PersonParameter::wisdom, person.def.values, "wisdom");
        assign_value(state, PersonParameter::strength, person.def.values, "strength");
        assign_value(state, PersonParameter::constitution, person.def.values, "constitution");
        assign_value(state, PersonParameter::dexterity, person.def.values, "dexterity");
        assign_value(state, PersonParameter::luck, person.def.values, "luck");
        assign_value(state, PersonParameter::perception, person.def.values, "perception");
        assign_value(state, PersonParameter::life, person.def.values, "life");
        assign_value(state, PersonParameter::energy, person.def.values, "energy");
        state.present[value_index(PersonParameter::maximum_life)] = true;
        for (const auto &[key, text] : person.def.values) {
            if (key.starts_with("skills."))
                state.skills.emplace(key.substr(7), parse_number(text));
        }
        add(std::move(state));
    }

    return runtime;
}

const PersonState *PersonRuntime::find(std::string_view person) const {
    const auto it = index_.find(person);
    return it == index_.end() ? nullptr : &persons_[it->second];
}

PersonState *PersonRuntime::find(std::string_view person) {
    const auto it = index_.find(person);
    return it == index_.end() ? nullptr : &persons_[it->second];
}

double PersonRuntime::get_parameter(std::string_view person, PersonParameter parameter) const {
    const PersonState *state = find(person);
    if (state == nullptr || parameter == PersonParameter::unknown)
        return 0.0;
    const std::size_t index = value_index(parameter);
    if (index >= state->present.size() || !state->present[index])
        return 0.0;

    switch (parameter) {
    case PersonParameter::experience:
        return experience_value(*state);
    case PersonParameter::reputation:
        return reputation_value(*state);
    case PersonParameter::intelligence:
        return intelligence_value(*state);
    case PersonParameter::wisdom:
        return wisdom_value(*state);
    case PersonParameter::strength:
        return strength_value(*state);
    case PersonParameter::constitution:
        return constitution_value(*state);
    case PersonParameter::dexterity:
        return dexterity_value(*state);
    case PersonParameter::luck:
        return luck_value(*state);
    case PersonParameter::perception:
        return perception_value(*state);
    case PersonParameter::maximum_life:
        return maximum_life_value(*state);
    case PersonParameter::life:
        return life_value(*state);
    case PersonParameter::energy:
        return energy_value(*state);
    case PersonParameter::unknown:
        break;
    }
    return 0.0;
}

void PersonRuntime::set_parameter(std::string_view person, PersonParameter parameter,
                                  double value) {
    PersonState *state = find(person);
    if (state == nullptr)
        return;

    switch (parameter) {
    case PersonParameter::unknown:
    case PersonParameter::experience:
    case PersonParameter::maximum_life:
        return;
    default:
        break;
    }

    state->present[value_index(parameter)] = true;
    switch (parameter) {
    case PersonParameter::life:
        state->values[value_index(PersonParameter::life)] = value;
        return;
    case PersonParameter::energy:
        state->values[value_index(PersonParameter::energy)] = value;
        return;
    case PersonParameter::reputation:
        set_reputation(*state, value);
        return;
    case PersonParameter::strength:
    case PersonParameter::constitution:
    case PersonParameter::dexterity:
    case PersonParameter::perception:
    case PersonParameter::intelligence:
    case PersonParameter::wisdom:
    case PersonParameter::luck:
        set_primary(*state, parameter, value);
        return;
    default:
        return;
    }
}

double PersonRuntime::get_skill(std::string_view person, std::string_view skill) const {
    const PersonState *state = find(person);
    if (state == nullptr)
        return 0.0;
    return clamp_value(skill_value(*state, skill), 0.0, kMaxSkill);
}

gl::script::PersonAccess PersonRuntime::access() {
    gl::script::PersonAccess access;
    access.get_parameter = [this](std::string_view person, PersonParameter parameter) {
        return get_parameter(person, parameter);
    };
    access.set_parameter = [this](std::string_view person, PersonParameter parameter,
                                  double value) { set_parameter(person, parameter, value); };
    access.get_skill = [this](std::string_view person, std::string_view skill) {
        return get_skill(person, skill);
    };
    return access;
}
} // namespace gl::game
