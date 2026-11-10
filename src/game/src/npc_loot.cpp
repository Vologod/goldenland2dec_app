#include "gl/game/npc_loot.hpp"

#include "gl/assets/log.hpp"
#include "gl/game/special_engine.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace gl::game {
namespace {
constexpr float kChanceScale = 0.01f;

bool in_window(const gl::assets::InvItem &item, std::int32_t loot_level,
               std::int32_t level_offset) {
    return item.min_level + level_offset <= loot_level &&
           loot_level <= item.max_level + level_offset;
}

std::int32_t scaled_bound(std::int32_t value, float scale) {
    const float product = static_cast<float>(value) * scale;
    std::int32_t truncated = static_cast<std::int32_t>(product);
    if (static_cast<float>(truncated) != product)
        truncated = static_cast<std::int32_t>(product + 1.0f);
    return truncated;
}

void append_chunks(std::vector<LootItem> &out, std::uint32_t item_id, std::int32_t quantity,
                   const ItemTemplate &item_template, std::uint32_t flags,
                   const gl::assets::SpecialDatabase *specials, std::int32_t loot_level,
                   MsvcRandom &rng) {
    if (item_template.max_stack == 0)
        return;
    std::int32_t remaining = quantity;
    while (remaining > 0) {
        const std::int32_t chunk =
            std::min(remaining, static_cast<std::int32_t>(item_template.max_stack));
        LootItem loot{item_id, chunk, item_template.type, flags, item_template.style};
        loot.mod_count = item_template.mod_count;
        if (specials != nullptr && chunk == 1 && (flags & 0x4u) != 0 &&
            item_template.mod_count == 0) {
            const int nspec = random_special_count(flags, rng);
            if (nspec > 0) {
                loot.mods = SpecialEngine::apply(
                    *specials, static_cast<gl::assets::ItemType>(item_template.type), nspec,
                    loot_level, rng);
                loot.flags |= 0x200u;
                loot.mod_count += static_cast<std::uint32_t>(loot.mods.size());
            }
        }
        out.push_back(std::move(loot));
        remaining -= chunk;
    }
}

std::optional<std::pair<std::uint32_t, ItemTemplate>>
resolve(const gl::assets::InvItem &item, const gl::assets::ItemNameIndex &names,
        const NpcLootGenerator::ItemTemplateLookup &lookup) {
    if (!lookup)
        return std::nullopt;
    const auto item_id = names.find(item.tech_name);
    if (!item_id)
        return std::nullopt;
    const auto item_template = lookup(*item_id);
    if (!item_template)
        return std::nullopt;
    return std::make_pair(*item_id, *item_template);
}
} // namespace

NpcLoot NpcLootGenerator::generate(const gl::assets::InvTable &table, std::int32_t loot_level,
                                   const gl::assets::ItemNameIndex &names,
                                   ItemTemplateLookup item_template, MsvcRandom &rng,
                                   const gl::assets::SpecialDatabase *specials) {
    NpcLoot loot;
    rng.coin(table.regenerate_chance);

    for (const gl::assets::InvItem &item : table.items) {
        if (!in_window(item, loot_level, table.level_offset))
            continue;
        const float chance = kChanceScale * static_cast<float>(item.chance_percent);
        if (!rng.coin(static_cast<double>(chance)))
            continue;
        if (item.chance_percent == 0.0)
            continue;
        const std::int32_t quantity = rng.range(item.qty_min, item.qty_max);
        if (quantity < 1)
            continue;
        const auto resolved = resolve(item, names, item_template);
        if (!resolved)
            continue;
        append_chunks(loot.hot, resolved->first, quantity, resolved->second,
                      resolved->second.flags & ~0x80u, specials, loot_level, rng);
    }

    if (table.chance_decrease == 0.0 || table.quantity_decrease == 0.0)
        return loot;

    const float chance_decrease = static_cast<float>(table.chance_decrease);
    const float quantity_scale = 1.0f / static_cast<float>(table.quantity_decrease);

    for (const gl::assets::InvItem &item : table.items) {
        if (!in_window(item, loot_level, table.level_offset))
            continue;
        const float chance = static_cast<float>(item.chance_percent);
        if (chance != 0.0f) {
            const float probability = (kChanceScale * chance) / chance_decrease;
            if (!rng.coin(static_cast<double>(probability)))
                continue;
        }
        const std::int32_t quantity = rng.range(scaled_bound(item.qty_min, quantity_scale),
                                                scaled_bound(item.qty_max, quantity_scale));
        if (quantity < 1)
            continue;
        const auto resolved = resolve(item, names, item_template);
        if (!resolved)
            continue;
        append_chunks(loot.main, resolved->first, quantity, resolved->second,
                      resolved->second.flags, specials, loot_level, rng);
    }

    return loot;
}

void generate_npc_inventories(InventoryRuntime &runtime, const std::vector<PersonInstance> &persons,
                              gl::assets::GameVfs &vfs, const gl::assets::ItemNameIndex &names,
                              InventoryRuntime::ItemTemplateLookup item_template,
                              std::int32_t loot_level, MsvcRandom &rng,
                              const gl::assets::SpecialDatabase *specials) {
    runtime.clear_persons();
    std::map<std::string, std::optional<gl::assets::InvTable>, std::less<>> cache;
    for (const PersonInstance &person : persons) {
        if (person.inventory.empty() || person.name == "Hero")
            continue;
        auto cached = cache.find(person.inventory);
        if (cached == cache.end()) {
            auto table = gl::assets::load_inv(vfs, person.inventory);
            if (!table) {
                gl::log(gl::LogLevel::warning, "npc inventory: cannot load '" + person.inventory +
                                                   "': " + table.error().message);
                cached = cache.emplace(person.inventory, std::nullopt).first;
            } else {
                cached = cache.emplace(person.inventory, std::move(table.value())).first;
            }
        }
        if (!cached->second)
            continue;
        const NpcLoot loot = NpcLootGenerator::generate(*cached->second, loot_level, names,
                                                        item_template, rng, specials);
        std::vector<gl::assets::InventoryItem> hot;
        hot.reserve(loot.hot.size());
        for (const LootItem &item : loot.hot) {
            gl::assets::InventoryItem converted{item.item_id,
                                                static_cast<std::uint32_t>(item.count),
                                                item.flags,
                                                item.type,
                                                item.style,
                                                100,
                                                item.mod_count};
            converted.mods = item.mods;
            hot.push_back(std::move(converted));
        }
        std::vector<gl::assets::InventoryItem> main;
        main.reserve(loot.main.size());
        for (const LootItem &item : loot.main) {
            gl::assets::InventoryItem converted{item.item_id,
                                                static_cast<std::uint32_t>(item.count),
                                                item.flags,
                                                item.type,
                                                item.style,
                                                100,
                                                item.mod_count};
            converted.mods = item.mods;
            main.push_back(std::move(converted));
        }
        runtime.set_person_trade_items(person.name, std::move(hot));
        runtime.set_person_items(person.name, std::move(main));
    }
}
} // namespace gl::game
