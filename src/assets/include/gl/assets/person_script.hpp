#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <map>
#include <span>
#include <string>

namespace gl::assets {
struct PersonDef {
    std::string res_name;
    std::int32_t level = 0;
    std::map<std::string, std::string> values;
};

Result<PersonDef> parse_person_script(std::span<const std::uint8_t> data);
} // namespace gl::assets
