#pragma once
#include "gl/assets/hero_stats.hpp"
#include "gl/game/person.hpp"
#include "gl/script/person_access.hpp"

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
struct PersonState {
    std::string name;
    std::array<double, 12> values{};
    std::array<bool, 12> present{};
    double strength_bonus = 0.0;
    bool modifier_gate = false;
    std::map<std::string, double, std::less<>> skills;
};

class PersonRuntime {
  public:
    static PersonRuntime create(const std::vector<PersonInstance> &persons,
                                const std::optional<gl::assets::HeroStats> &hero);
    double get_parameter(std::string_view person, gl::script::PersonParameter parameter) const;
    void set_parameter(std::string_view person, gl::script::PersonParameter parameter,
                       double value);
    double get_skill(std::string_view person, std::string_view skill) const;
    gl::script::PersonAccess access();

  private:
    const PersonState *find(std::string_view person) const;
    PersonState *find(std::string_view person);

    std::vector<PersonState> persons_;
    std::map<std::string, std::size_t, std::less<>> index_;
};
} // namespace gl::game
