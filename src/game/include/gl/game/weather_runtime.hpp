#pragma once
#include "gl/script/weather_access.hpp"

#include <cstdint>

namespace gl::game {
class WeatherRuntime {
  public:
    void set_from_script(std::int32_t code);
    std::uint32_t current() const { return current_; }
    void clear() { current_ = 0; }

    gl::script::WeatherAccess access();

  private:
    std::uint32_t current_ = 0;
};
} // namespace gl::game
