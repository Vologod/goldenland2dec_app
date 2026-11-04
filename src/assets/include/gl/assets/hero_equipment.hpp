#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
struct EquippedItem {
    std::uint32_t item_id = 0;
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t style = 0;
};

struct HeroEquipment {
    std::vector<EquippedItem> items;
};

Result<std::vector<std::uint8_t>> decode_save_body(std::span<const std::uint8_t> file);
Result<HeroEquipment> parse_hero_equipment(std::span<const std::uint8_t> body);
} // namespace gl::assets
