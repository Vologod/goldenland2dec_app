#pragma once
#include "gl/assets/result.hpp"
#include "gl/assets/save_item.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
struct HeroContainerState {
    std::uint32_t flag = 0;
    std::vector<SaveItemRecord> items;
};

struct HeroSaveBody {
    std::uint32_t tile_x = 0;
    std::uint32_t tile_y = 0;
    std::uint32_t direction = 0;
    std::array<std::uint8_t, 0x11C> stat_block{};
    std::uint32_t hp = 0;
    std::uint32_t ep = 0;
    std::uint32_t ap = 0;
    std::array<std::uint8_t, 0x600> buffer600{};
    std::array<HeroContainerState, 3> containers;
    std::uint32_t field_2bb4 = 0;
    std::uint32_t field_2bb8 = 0;
    std::array<std::uint8_t, 0x80> tail80{};
    std::vector<std::array<std::uint8_t, 0x14>> extra;
    std::uint32_t attached_count = 0;
    std::vector<std::uint8_t> attached_raw;
};

Result<HeroSaveBody> parse_hero_save_body(std::span<const std::uint8_t> data);
Result<std::vector<std::uint8_t>> encode_hero_save_body(const HeroSaveBody &body);
} // namespace gl::assets
