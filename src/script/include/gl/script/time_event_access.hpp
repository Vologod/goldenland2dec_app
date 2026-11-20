#pragma once
#include <cstdint>
#include <functional>

namespace gl::script {
struct TimeEventAccess {
    std::function<std::int32_t(std::int32_t, std::int32_t)> set_event;
    std::function<std::int32_t(std::int32_t)> get_event;
    std::function<void(std::int32_t)> clear_event;
};
} // namespace gl::script
