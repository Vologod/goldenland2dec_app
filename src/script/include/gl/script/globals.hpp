#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/script/value.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gl::script {
class ScriptVm;

struct ScriptGlobal {
    std::string type;
    std::string name;
    Value value;
};

gl::Result<std::vector<ScriptGlobal>> parse_globals(std::span<const std::uint8_t> data);

std::vector<ScriptGlobal> save_globals_from_vars(
    std::span<const gl::assets::SaveVar> vars);

std::vector<gl::assets::SaveVar> update_vars_from_globals(
    const std::vector<gl::assets::SaveVar> &saved, const ScriptVm &vm);
}
