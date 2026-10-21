#pragma once
#include "gl/script/value.hpp"
#include <cstdint>
#include <span>

namespace gl::script {
class ScriptHost {
public:
    virtual ~ScriptHost() = default;
    virtual Value call(std::uint32_t id, std::span<const Value> args) = 0;
};
}
