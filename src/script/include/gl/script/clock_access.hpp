#pragma once
#include <cstdint>
#include <functional>

namespace gl::script {
struct ClockAccess {
    std::function<std::int32_t()> current_hour;
    std::function<std::int32_t()> days_from_beginning;
};
} // namespace gl::script
