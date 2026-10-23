#pragma once
#include "gl/game/person.hpp"
#include <optional>
#include <string>

namespace gl::game {
std::optional<std::string> dialog_script_path(const PersonInstance& person);
} // namespace gl::game
