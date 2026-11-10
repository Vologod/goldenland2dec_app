#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/inv_table.hpp"
#include "gl/assets/item_name_index.hpp"
#include "gl/assets/specials.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/msvc_random.hpp"
#include "gl/game/person.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace gl::assets {
class GameVfs;
}

namespace gl::game {
struct LootItem {
    LootItem() = default;
    LootItem(std::uint32_t id, std::int32_t item_count, std::uint32_t item_type,
             std::uint32_t item_flags, std::uint32_t item_style)
        : item_id(id), count(item_count), type(item_type), flags(item_flags), style(item_style) {}

    std::uint32_t item_id = 0;
    std::int32_t count = 0;
    std::uint32_t type = 0;
    std::uint32_t flags = 0;
    std::uint32_t style = 0;
    std::vector<gl::assets::SpecialMod> mods;
};

struct NpcLoot {
    std::vector<LootItem> hot;
    std::vector<LootItem> main;
};

struct NpcLootGenerator {
    using ItemTemplateLookup = std::function<std::optional<ItemTemplate>(std::uint32_t)>;

    static NpcLoot generate(const gl::assets::InvTable &table, std::int32_t loot_level,
                            const gl::assets::ItemNameIndex &names,
                            ItemTemplateLookup item_template, MsvcRandom &rng,
                            const gl::assets::SpecialDatabase *specials = nullptr);
};

void generate_npc_inventories(InventoryRuntime &runtime, const std::vector<PersonInstance> &persons,
                              gl::assets::GameVfs &vfs, const gl::assets::ItemNameIndex &names,
                              InventoryRuntime::ItemTemplateLookup item_template,
                              std::int32_t loot_level, MsvcRandom &rng,
                              const gl::assets::SpecialDatabase *specials = nullptr);
} // namespace gl::game
