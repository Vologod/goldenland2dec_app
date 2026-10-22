#pragma once
#include "gl/game/walk_grid.hpp"
#include <cstdint>
#include <vector>

namespace gl::game {
struct Tile { std::int32_t x = 0, y = 0; };
inline bool operator==(Tile a, Tile b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(Tile a, Tile b) { return !(a == b); }

bool line_of_sight(const WalkGrid& grid, Tile from, Tile to,
                   const std::vector<std::uint8_t>& blocked);
std::vector<Tile> find_path(const WalkGrid& grid, Tile from, Tile to,
                            const std::vector<std::uint8_t>& blocked);
}
