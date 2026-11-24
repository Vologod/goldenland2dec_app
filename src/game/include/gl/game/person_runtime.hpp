#pragma once
#include "gl/assets/hero_stats.hpp"
#include "gl/game/person.hpp"
#include "gl/script/person_access.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
int hero_level_from_experience(double experience);

struct PersonEffect {
    std::array<std::int32_t, 5> words{};
};

void set_injured_records(std::vector<PersonEffect> &records, bool injured);

struct PersonState {
    std::string name;
    std::array<double, 12> values{};
    std::array<bool, 12> present{};
    double strength_bonus = 0.0;
    bool modifier_gate = false;
    std::map<std::string, double, std::less<>> skills;
    bool undead = false;
    std::int32_t party_role = 0;
    std::vector<PersonEffect> effects;
};

class PersonRuntime {
  public:
    static PersonRuntime create(const std::vector<PersonInstance> &persons,
                                const std::optional<gl::assets::HeroStats> &hero);
    double get_parameter(std::string_view person, gl::script::PersonParameter parameter) const;
    double maximum_energy(std::string_view person) const;
    bool has_parameter(std::string_view person, gl::script::PersonParameter parameter) const;
    void set_parameter(std::string_view person, gl::script::PersonParameter parameter,
                       double value);
    double get_skill(std::string_view person, std::string_view skill) const;
    bool has_person(std::string_view person) const;
    void set_undead(std::string_view person, bool undead);
    bool undead(std::string_view person) const;
    void set_party_role(std::string_view person, std::int32_t role);
    std::int32_t party_role(std::string_view person) const;
    void set_injured(std::string_view person, bool injured);
    bool injured(std::string_view person) const;
    std::int32_t dialog_percent(std::string_view person) const;
    gl::script::PersonAccess access();

  private:
    const PersonState *find(std::string_view person) const;
    PersonState *find(std::string_view person);

    std::vector<PersonState> persons_;
    std::map<std::string, std::size_t, std::less<>> index_;
};
} // namespace gl::game
