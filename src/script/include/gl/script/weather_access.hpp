#pragma once
#include <cstdint>
#include <functional>

namespace gl::script {
struct WeatherAccess {
    std::function<void(std::int32_t)> set_weather;
};
} // namespace gl::script
