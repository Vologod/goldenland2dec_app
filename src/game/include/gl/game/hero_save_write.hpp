#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/hero_save_body.hpp"
#include "gl/assets/hero_stats.hpp"
#include "gl/game/effect.hpp"
#include <array>
#include <cstdint>
#include <optional>
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
    std::vector<PersonEffect> effects;
    bool has_effects = false;
    std::optional<GameDatetime> effect_clock;
    std::optional<gl::assets::HeroStats> stats;
};

gl::assets::HeroSaveBody make_hero_save_body(const gl::assets::HeroSaveBody &loaded,
                                             const HeroLiveState &live);
} // namespace gl::game
