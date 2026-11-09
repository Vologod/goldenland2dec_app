#pragma once
#include "gl/assets/hero_equipment.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace gl::game {

struct InventoryViewState {
    std::uint32_t filter_mask = 0;
    std::size_t page = 0;
    std::optional<std::size_t> selected;
};

std::vector<std::size_t> visible_inventory(const gl::assets::HeroInventory &inventory,
                                           std::uint32_t mask);
std::size_t clamp_page(std::size_t page, std::size_t count);
bool item_matches_filter(std::uint32_t type, std::uint32_t mask);

} // namespace gl::game
