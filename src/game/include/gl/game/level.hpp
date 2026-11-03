#pragma once
#include "gl/assets/image.hpp"
#include "gl/assets/result.hpp"
#include "gl/assets/sef.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/person.hpp"
#include "gl/game/walk_grid.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gl::game {
struct LevelStatic {
    std::uint32_t number = 0;
    std::int32_t x = 0, y = 0;
    gl::assets::Image image;
};

struct LevelAnimation {
    std::uint32_t number = 0;
    std::int32_t x = 0, y = 0;
    std::uint32_t delay_ms = 0;
    std::vector<gl::assets::Image> frames;
};

struct LevelMask {
    std::uint32_t number = 0;
    std::int32_t x = 0, y = 0;
    gl::assets::Image image;
};

struct MaskSubcell {
    std::uint32_t flags = 0;
    std::uint16_t mask = 0xFFFF;
};

struct MaskCellGrid {
    int width = 0;
    int height = 0;
    std::vector<std::array<MaskSubcell, 4>> cells;

    const std::array<MaskSubcell, 4>* at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return nullptr;
        return &cells[static_cast<std::size_t>(y) * width + x];
    }
};

struct Level {
    std::string name, pack;
    std::uint32_t width = 0, height = 0;
    gl::assets::Image background;
    std::vector<LevelStatic> statics;
    std::vector<LevelAnimation> animations;
    std::vector<LevelMask> masks;
    std::vector<std::uint32_t> mask_numbers;
    MaskCellGrid mask_cells;
    std::vector<PersonInstance> persons;
    std::vector<gl::assets::SefEntrance> entrances;
    WalkGrid walkable;
};

gl::Result<Level> load_level(gl::assets::GameVfs &vfs, std::string_view level_name);
} // namespace gl::game
