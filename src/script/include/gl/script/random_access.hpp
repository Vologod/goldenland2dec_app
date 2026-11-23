#pragma once
#include <cstdint>
#include <functional>

namespace gl::script {
struct RandomAccess {
    std::function<std::int32_t(std::int32_t, std::int32_t)> range;
    std::function<bool(double)> coin;
};
} // namespace gl::script
