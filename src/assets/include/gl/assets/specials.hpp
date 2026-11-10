#pragma once
#include "gl/assets/itm.hpp"
#include "gl/assets/result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace gl::assets {
class GameVfs;

inline constexpr std::size_t special_value_slot_count = 3;
inline constexpr std::size_t special_time_slot_count = 6;
inline constexpr std::size_t special_slot_count =
    special_value_slot_count + special_time_slot_count;

using SpecialId = std::uint32_t;
using SpecialFlags = std::uint32_t;

struct SpecialSlot {
    std::uint32_t tag = 0;
    std::uint8_t access = 0;
    float value = 0.0f;
    float price = 0.0f;
};

constexpr bool is_special_value_tag(SpecialFlags tag) {
    return tag == 0x40 || tag == 0x80 || tag == 0x100;
}

constexpr bool is_special_time_tag(SpecialFlags tag) {
    return tag == 1 || tag == 2 || tag == 4 || tag == 8 || tag == 0x10 || tag == 0x20;
}

struct SpecialDef {
    SpecialId id = 0;
    bool random_sign = false;
    std::array<SpecialSlot, special_value_slot_count> value_slots{};
    std::array<SpecialSlot, special_time_slot_count> time_slots{};
};

struct SpecialDatabase {
    std::vector<SpecialDef> defs;
    std::map<ItemType, std::vector<SpecialId>> item_classes;

    const SpecialDef *find(SpecialId id) const;
};

Result<SpecialDatabase> parse_specials_scr(std::span<const std::uint8_t> data);
Result<SpecialDatabase> load_specials_scr(GameVfs &vfs);

std::optional<SpecialId> special_id_from_name(std::string_view name);
std::optional<SpecialFlags> special_flag_from_name(std::string_view name);
std::optional<ItemType> special_item_type_from_name(std::string_view name);

std::size_t special_id_name_count();
std::size_t special_flag_name_count();
std::size_t special_item_type_name_count();
} // namespace gl::assets
