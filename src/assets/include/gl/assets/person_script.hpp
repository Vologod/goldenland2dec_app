#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace gl::assets {
struct PersonWeapon {
    std::string name;
    std::int32_t min_level = 0;
    std::int32_t max_level = 0;
};

struct PersonDef {
    std::string res_name;
    std::int32_t level = 0;
    bool has_weapon = false;
    std::int32_t weapon_level_offset = 0;
    std::vector<PersonWeapon> weapons;
    std::map<std::string, std::string> values;
};

Result<PersonDef> parse_person_script(std::span<const std::uint8_t> data);
} // namespace gl::assets
