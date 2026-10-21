#pragma once
#include "gl/assets/result.hpp"
#include "gl/script/value.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gl::script {
struct ScriptGlobal {
    std::string type;
    std::string name;
    Value value;
};

gl::Result<std::vector<ScriptGlobal>> parse_globals(std::span<const std::uint8_t> data);
}
