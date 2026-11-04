#include "gl/game/mask_selection.hpp"
#include <doctest/doctest.h>

using namespace gl::game;

namespace {
MaskCellGrid make_grid(int w, int h, std::uint32_t flags, std::uint16_t mask) {
    MaskCellGrid grid;
    grid.width = w;
    grid.height = h;
    grid.cells.resize(static_cast<std::size_t>(w) * h);
    for (auto &cell : grid.cells)
        for (auto &sub : cell) {
            sub.flags = flags;
            sub.mask = mask;
        }
    return grid;
}
} // namespace

TEST_CASE("select_cell_mask ignores cells without a mask") {
    auto grid = make_grid(3, 3, 0, 0xFFFF);
    auto selection = select_cell_mask(grid, 1, 1);
    for (std::size_t s = 0; s < 4; ++s)
        CHECK(selection.applies[s] == 0);
}

TEST_CASE("select_cell_mask requires continuation right and down") {
    auto grid = make_grid(3, 3, 1, 0);
    auto selection = select_cell_mask(grid, 1, 1);
    for (std::size_t s = 0; s < 4; ++s)
        CHECK(selection.applies[s] == 0);
}

TEST_CASE("select_cell_mask applies an interior masked subcell") {
    auto grid = make_grid(3, 3, 3, 7);
    auto selection = select_cell_mask(grid, 1, 1);
    for (std::size_t s = 0; s < 4; ++s) {
        CHECK(selection.applies[s] == 1);
        CHECK(selection.mask_index[s] == 7);
    }
}

TEST_CASE("select_row_mask_ids collects ids across the person row") {
    auto grid = make_grid(3, 2, 3, 7);
    const std::vector<std::uint16_t> ids = select_row_mask_ids(grid, 0, 2, 0);
    CHECK(ids == std::vector<std::uint16_t>{7});
}

TEST_CASE("select_cell_mask handles invalid coordinates") {
    auto grid = make_grid(2, 2, 3, 0);
    auto selection = select_cell_mask(grid, 5, 5);
    for (std::size_t s = 0; s < 4; ++s)
        CHECK(selection.applies[s] == 0);
}
