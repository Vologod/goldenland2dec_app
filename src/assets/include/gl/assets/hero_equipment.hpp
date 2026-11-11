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

struct SpecialMod {
    std::uint32_t id = 0;
    std::int32_t value = 0;
    std::uint32_t flags = 0;
};

struct InventoryItem {
    InventoryItem() = default;
    InventoryItem(std::uint32_t id, std::uint32_t item_count, std::uint32_t item_flags,
                  std::uint32_t item_type, std::uint32_t item_style,
                  std::uint32_t item_durability = 100, std::uint32_t item_mod_count = 0)
        : item_id(id), count(item_count), flags(item_flags), type(item_type), style(item_style),
          durability(item_durability), mod_count(item_mod_count) {}

    std::uint32_t item_id = 0;
    std::uint32_t count = 1;
    std::uint32_t flags = 0;
    std::uint32_t type = 0;
    std::uint32_t style = 0;
    std::uint32_t durability = 100;
    // Saves store only the mod count. For added/generated records,
    // mod_count == mods.size() (generated specials and inherited ITM prototype
    // effects both populate mods); save-loaded records keep the count-only value.
    std::uint32_t mod_count = 0;
    std::vector<SpecialMod> mods;
};

struct HeroInventory {
    std::vector<InventoryItem> hot;
    std::vector<InventoryItem> items;
    std::vector<InventoryItem> equipped;
};

Result<std::vector<std::uint8_t>> decode_save_body(std::span<const std::uint8_t> file);
Result<HeroEquipment> parse_hero_equipment(std::span<const std::uint8_t> body);
Result<HeroInventory> parse_hero_inventory(std::span<const std::uint8_t> body);
} // namespace gl::assets
