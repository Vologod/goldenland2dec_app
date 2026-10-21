#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace gl::script {
using Value = std::variant<std::int32_t, std::uint32_t, double, std::string>;
}
