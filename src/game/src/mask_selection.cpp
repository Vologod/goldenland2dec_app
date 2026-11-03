#include "gl/game/mask_selection.hpp"

namespace gl::game {

CellMaskSelection select_cell_mask(const MaskCellGrid &grid, int cell_x, int cell_y) {
    CellMaskSelection result;
    const auto *cell = grid.at(cell_x, cell_y);
    if (cell == nullptr)
        return result;

    for (std::size_t sub = 0; sub < 4; ++sub) {
        const std::uint16_t id = (*cell)[sub].mask;
        if (id == 0xFFFF)
            continue;

        bool right = false;
        for (int x = cell_x; x < grid.width; ++x) {
            const auto *scan = grid.at(x, cell_y);
            if (scan == nullptr)
                break;
            const MaskSubcell &s = (*scan)[sub];
            if ((s.flags & 1u) == 0 || s.mask != id)
                break;
            if ((s.flags & 2u) != 0) {
                right = true;
                break;
            }
        }

        bool down = false;
        for (int y = cell_y; y < grid.height; ++y) {
            const auto *scan = grid.at(cell_x, y);
            if (scan == nullptr)
                break;
            const MaskSubcell &s = (*scan)[sub];
            if ((s.flags & 1u) == 0 || s.mask != id)
                break;
            if ((s.flags & 2u) != 0) {
                down = true;
                break;
            }
        }

        if (right && down) {
            result.applies[sub] = 1;
            result.mask_index[sub] = id;
        }
    }
    return result;
}

} // namespace gl::game
