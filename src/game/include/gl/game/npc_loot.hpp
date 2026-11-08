#pragma once
#include "gl/assets/inv_table.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/msvc_random.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace gl::game {
struct LootItem {
    std::uint32_t item_id = 0;
    std::int32_t count = 0;
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t style = 0;
};

struct NpcLoot {
    std::vector<LootItem> hot;
    std::vector<LootItem> main;
};

struct NpcLootGenerator {
    using ItemTemplateLookup = std::function<std::optional<ItemTemplate>(std::uint32_t)>;

    static NpcLoot generate(const gl::assets::InvTable &table, std::int32_t loot_level,
                            const gl::assets::ItemNameIndex &names,
                            ItemTemplateLookup item_template, MsvcRandom &rng);
};
} // namespace gl::game
