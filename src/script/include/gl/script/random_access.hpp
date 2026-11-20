#pragma once
#include <cstdint>
#include <functional>

namespace gl::script {
struct RandomAccess {
    std::function<std::int32_t(std::int32_t, std::int32_t)> range;
};
} // namespace gl::script
