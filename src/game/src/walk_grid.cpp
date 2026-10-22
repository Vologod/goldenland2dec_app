#include "gl/game/walk_grid.hpp"

namespace gl::game {
bool WalkGrid::is_walkable(std::int32_t x, std::int32_t y) const {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return false;
    return walkable[static_cast<std::size_t>(y) * width + x] != 0;
}

WalkGrid build_walk_grid(const gl::assets::LvlData &level) {
    WalkGrid grid;
    if (level.chunk_width == 0 || level.chunk_height == 0)
        return grid;
    grid.width = static_cast<std::int32_t>(level.chunk_width) * 2;
    grid.height = static_cast<std::int32_t>(level.chunk_height) * 2;
    grid.walkable.assign(static_cast<std::size_t>(grid.width) * grid.height, 0);
    for (std::uint32_t cx = 0; cx < level.chunk_width; ++cx) {
        for (std::uint32_t cy = 0; cy < level.chunk_height; ++cy) {
            const auto &chunk =
                level.chunks[static_cast<std::size_t>(cx) * level.chunk_height + cy];
            const std::uint32_t word = static_cast<std::uint32_t>(chunk[0].relief) |
                                       (static_cast<std::uint32_t>(chunk[0].sound) << 16);
            for (std::uint32_t t = 0; t < 4; ++t) {
                const std::uint32_t tx = cx * 2 + (t & 1u);
                const std::uint32_t ty = cy * 2 + (t >> 1u);
                const std::uint32_t sub = (tx & 1u) + 2u * (ty & 1u);
                const bool blocked = (word & (0x100u << sub)) != 0;
                grid.walkable[static_cast<std::size_t>(ty) * grid.width + tx] = blocked ? 0 : 1;
            }
        }
    }
    return grid;
}
} // namespace gl::game
