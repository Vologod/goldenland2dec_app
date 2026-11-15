#pragma once
#include "gl/assets/result.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gl::assets {
struct SaveItemModifier {
    std::array<std::uint32_t, 5> words{};
};

struct SaveItemRecord {
    std::uint32_t item_id = 0;
    std::uint32_t count = 0;
    std::uint32_t durability = 0;
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t style = 0;
    std::uint32_t mod_count = 0;
    std::vector<SaveItemModifier> modifiers;
};

struct ItemRecordRead {
    SaveItemRecord record;
    std::size_t size = 0;
};

Result<ItemRecordRead> parse_item_record(std::span<const std::uint8_t> data);
} // namespace gl::assets
