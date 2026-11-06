#pragma once
#include <cstdint>
#include <functional>
#include <string_view>

namespace gl::script {
enum class PersonParameter : std::uint8_t {
    experience,
    reputation,
    intelligence,
    wisdom,
    strength,
    constitution,
    dexterity,
    luck,
    perception,
    maximum_life,
    life,
    energy,
    unknown,
};

struct PersonAccess {
    std::function<double(std::string_view person, PersonParameter parameter)> get_parameter;
    std::function<void(std::string_view person, PersonParameter parameter, double value)>
        set_parameter;
    std::function<double(std::string_view person, std::string_view skill)> get_skill;
};

PersonParameter person_parameter_from_name(std::string_view name);
} // namespace gl::script
