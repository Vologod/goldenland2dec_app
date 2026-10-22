#include "gl/game/person_movement.hpp"
#include <array>
#include <cmath>

namespace gl::game {
namespace {
constexpr float kVertical = 0.9863f;
constexpr float kMostlyVertical = 0.8616f;
constexpr float kDiagonal = 0.5878f;
constexpr float kMostlyHorizontal = 0.2079f;
} // namespace

RouteType parse_route_type(std::string_view type) noexcept {
    if (type == "STAY_ROTATE")
        return RouteType::stay_rotate;
    if (type == "MOVED")
        return RouteType::moved;
    if (type == "MOVED_FLIP")
        return RouteType::moved_flip;
    if (type == "RANDOM_RADIUS")
        return RouteType::random_radius;
    if (type == "RANDOM")
        return RouteType::random;
    return RouteType::stay;
}

std::uint8_t direction16_from_velocity(float vx, float vy) noexcept {
    const float length = std::sqrt(vx * vx + vy * vy);
    if (length <= 0.0f)
        return 0;
    const float x = std::fabs(vx / length);
    const bool downward = vy > 0.0f;
    if (vx < 0.0f) {
        if (downward) {
            if (x <= kMostlyHorizontal)
                return 8;
            if (x <= kDiagonal)
                return 7;
            if (x <= kMostlyVertical)
                return 6;
            if (x <= kVertical)
                return 5;
            return 4;
        }
        if (x <= kMostlyHorizontal)
            return 0;
        if (x <= kDiagonal)
            return 1;
        if (x <= kMostlyVertical)
            return 2;
        if (x <= kVertical)
            return 3;
        return 4;
    }
    if (downward) {
        if (x <= kMostlyHorizontal)
            return 8;
        if (x <= kDiagonal)
            return 9;
        if (x <= kMostlyVertical)
            return 10;
        if (x <= kVertical)
            return 11;
        return 12;
    }
    if (x <= kMostlyHorizontal)
        return 0;
    if (x <= kDiagonal)
        return 15;
    if (x <= kMostlyVertical)
        return 14;
    if (x <= kVertical)
        return 13;
    return 12;
}

std::uint8_t direction16_to8(std::uint8_t direction16) noexcept {
    static constexpr std::array<std::uint8_t, 16> kMap{0, 1, 1, 1, 2, 3, 3, 3,
                                                       4, 5, 5, 5, 6, 7, 7, 7};
    return kMap[direction16 % kMap.size()];
}

std::uint8_t direction8_to16(std::uint8_t direction8) noexcept {
    static constexpr std::array<std::uint8_t, 8> kMap{0, 2, 4, 6, 8, 10, 12, 14};
    return kMap[direction8 % kMap.size()];
}

float iso_speed(float vx, float vy, float move_x, float move_y) noexcept {
    const float length2 = vx * vx + vy * vy;
    if (length2 <= 0.0f)
        return move_y;
    const float fx2 = (vx * vx) / length2;
    return move_y + (move_x - move_y) * fx2;
}
} // namespace gl::game
