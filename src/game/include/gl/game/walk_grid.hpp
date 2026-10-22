#pragma once
#include "gl/assets/lvl.hpp"
#include <cstdint>
#include <vector>

namespace gl::game {
struct WalkGrid {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::vector<std::uint8_t> walkable;
    bool is_walkable(std::int32_t x, std::int32_t y) const;
    bool empty() const { return width <= 0 || height <= 0; }
};
WalkGrid build_walk_grid(const gl::assets::LvlData &level);
} // namespace gl::game
