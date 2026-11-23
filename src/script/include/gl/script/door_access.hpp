#pragma once
#include <cstdint>
#include <functional>
#include <string_view>

namespace gl::script {
struct DoorAccess {
    std::function<bool(std::string_view, std::int32_t)> set;
};
} // namespace gl::script
