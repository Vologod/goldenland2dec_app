#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_save_body.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace gl::game {
struct HeroLiveState {
    std::uint32_t tile_x = 0;
    std::uint32_t tile_y = 0;
    std::uint32_t direction = 0;
    std::uint32_t hp = 0;
    std::uint32_t ep = 0;
    std::uint32_t ap = 0;
    std::array<std::vector<gl::assets::InventoryItem>, 3> containers;
    std::array<bool, 3> present{true, true, true};
};

gl::assets::HeroSaveBody make_hero_save_body(const gl::assets::HeroSaveBody &loaded,
                                             const HeroLiveState &live);
} // namespace gl::game
