#pragma once
#include "gl/assets/result.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gl::assets {
class GameVfs;

struct InvItem {
    std::string tech_name;
    std::int32_t min_level = 0;
    std::int32_t max_level = 0;
    double chance_percent = 0.0;
    std::int32_t qty_min = 0;
    std::int32_t qty_max = 0;
};

struct InvTable {
    double regenerate_chance = 1.0;
    double chance_decrease = 1.0;
    double quantity_decrease = 1.0;
    std::int32_t level_offset = 0;
    std::vector<InvItem> items;
};

Result<InvTable> parse_inv(std::span<const std::uint8_t> data);
Result<InvTable> load_inv(GameVfs &vfs, std::string_view name);
} // namespace gl::assets
