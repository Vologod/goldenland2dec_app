#pragma once
#include "gl/game/level.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace gl::game {

struct CellMaskSelection {
    std::array<std::uint8_t, 4> applies{};
    std::array<std::uint16_t, 4> mask_index{{0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}};
};

CellMaskSelection select_cell_mask(const MaskCellGrid &grid, int cell_x, int cell_y);

} // namespace gl::game
