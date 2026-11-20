#include "gl/game/weather_runtime.hpp"

namespace gl::game {
void WeatherRuntime::set_from_script(std::int32_t code) {
    current_ = code == 1 ? 0x10001u : code == 2 ? 0x10002u : 0u;
}

gl::script::WeatherAccess WeatherRuntime::access() {
    gl::script::WeatherAccess access;
    access.set_weather = [this](std::int32_t code) { set_from_script(code); };
    return access;
}
} // namespace gl::game
