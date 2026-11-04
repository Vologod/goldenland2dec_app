#include "gl/game/mask_selection.hpp"
#include <algorithm>

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

std::vector<std::uint16_t> select_row_mask_ids(const MaskCellGrid &grid, int cell_x0,
                                              int cell_x1, int cell_y) {
    std::vector<std::uint16_t> selected;
    for (int cx = cell_x0; cx <= cell_x1; ++cx) {
        const CellMaskSelection selection = select_cell_mask(grid, cx, cell_y);
        for (std::size_t sub = 0; sub < 4; ++sub) {
            if (selection.applies[sub] == 0 || selection.mask_index[sub] == 0xFFFF)
                continue;
            const std::uint16_t id = selection.mask_index[sub];
            if (std::find(selected.begin(), selected.end(), id) == selected.end())
                selected.push_back(id);
        }
    }
    return selected;
}

} // namespace gl::game
