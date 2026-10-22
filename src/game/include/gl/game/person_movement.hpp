#pragma once
#include <cstdint>
#include <string_view>

namespace gl::game {
enum class RouteType : std::uint8_t {
    stay = 0,
    stay_rotate = 1,
    moved = 2,
    moved_flip = 3,
    random_radius = 4,
    random = 5
};
RouteType parse_route_type(std::string_view type) noexcept;
std::uint8_t direction16_from_velocity(float vx, float vy) noexcept;
std::uint8_t direction16_to8(std::uint8_t direction16) noexcept;
std::uint8_t direction8_to16(std::uint8_t direction8) noexcept;
float iso_speed(float vx, float vy, float move_x, float move_y) noexcept;
} // namespace gl::game
