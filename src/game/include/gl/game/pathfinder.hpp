#pragma once
#include "gl/game/walk_grid.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace gl::game {
struct Tile { std::int32_t x = 0, y = 0; };
inline bool operator==(Tile a, Tile b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(Tile a, Tile b) { return !(a == b); }

inline constexpr std::int32_t kTileWidth = 12;
inline constexpr std::int32_t kTileHeight = 9;
inline constexpr std::int32_t kHeroMarkerSize = 20;
inline Tile tile_at(float x, float y) {
    return {static_cast<std::int32_t>(std::lround((x - 6.0f) / 12.0f)),
            static_cast<std::int32_t>(std::lround((y - 4.0f) / 9.0f))};
}
inline float tile_center_x(std::int32_t x) { return x * 12.0f + 6.0f; }
inline float tile_center_y(std::int32_t y) { return y * 9.0f + 4.0f; }

bool line_of_sight(const WalkGrid& grid, Tile from, Tile to,
                   const std::vector<std::uint8_t>& blocked);
std::vector<Tile> find_path(const WalkGrid& grid, Tile from, Tile to,
                            const std::vector<std::uint8_t>& blocked);
}
