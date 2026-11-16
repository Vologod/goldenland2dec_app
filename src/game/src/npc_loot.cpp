#include "gl/game/npc_loot.hpp"

#include "gl/assets/log.hpp"
#include "gl/game/person_rng.hpp"
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
    auto add_chunk = [&](std::int32_t chunk) -> LootItem & {
        if (item_template.max_stack >= 2) {
            for (LootItem &existing : out) {
                if (existing.item_id == item_id &&
                    static_cast<std::uint32_t>(existing.count) < item_template.max_stack) {
                    existing.count = chunk;
                    return existing;
                }
            }
        }
        LootItem loot{item_id, chunk, item_template.type, flags, item_template.style};
        loot.mod_count = item_template.mod_count;
        loot.mods.reserve(item_template.effects.size());
        for (const ItemTemplateEffect &effect : item_template.effects)
            loot.mods.push_back({effect.type, effect.value, effect.flags});
        out.push_back(std::move(loot));
        return out.back();
    };
    std::int32_t remaining = quantity;
    while (remaining > 0) {
        const std::int32_t chunk =
            std::min(remaining, static_cast<std::int32_t>(item_template.max_stack));
        LootItem &loot = add_chunk(chunk);
        if (specials != nullptr && chunk == 1 && (loot.flags & 0x4u) != 0 && loot.mod_count == 0) {
            const int nspec = random_special_count(flags, rng);
            if (nspec > 0) {
                loot.mods = SpecialEngine::apply(
                    *specials, static_cast<gl::assets::ItemType>(item_template.type), nspec,
                    loot_level, rng);
                loot.flags |= 0x200u;
                loot.mod_count += static_cast<std::uint32_t>(loot.mods.size());
            }
        }
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

std::optional<NpcLoot> NpcLootGenerator::generate(const gl::assets::InvTable &table,
                                                  std::int32_t loot_level,
                                                  const gl::assets::ItemNameIndex &names,
                                                  ItemTemplateLookup item_template, MsvcRandom &rng,
                                                  const gl::assets::SpecialDatabase *specials,
                                                  bool force) {
    NpcLoot loot;
    if (!rng.coin(table.regenerate_chance) && !force)
        return std::nullopt;

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

namespace {
std::vector<gl::assets::InventoryItem> convert_loot(const std::vector<LootItem> &items) {
    std::vector<gl::assets::InventoryItem> converted;
    converted.reserve(items.size());
    for (const LootItem &item : items) {
        gl::assets::InventoryItem entry{item.item_id,
                                        static_cast<std::uint32_t>(item.count),
                                        item.flags, item.type, item.style, 100,
                                        item.mod_count};
        entry.mods = item.mods;
        converted.push_back(std::move(entry));
    }
    return converted;
}

NpcInventoryCounts generate_npc_indices(
    InventoryRuntime &runtime, const std::vector<PersonInstance> &persons,
    const std::vector<std::size_t> &indices, gl::assets::GameVfs &vfs,
    const gl::assets::ItemNameIndex &names,
    InventoryRuntime::ItemTemplateLookup item_template, std::int32_t loot_level,
    MsvcRandom &rng, const gl::assets::SpecialDatabase *specials, bool force,
    bool consume_init_draws) {
    NpcInventoryCounts counts;
    counts.requested = indices.size();
    std::map<std::string, std::optional<gl::assets::InvTable>, std::less<>> cache;
    for (std::size_t index : indices) {
        const PersonInstance &person = persons[index];
        if (person.name == "Hero")
            continue;
        if (!person.inventory.empty()) {
            auto cached = cache.find(person.inventory);
            if (cached == cache.end()) {
                auto table = gl::assets::load_inv(vfs, person.inventory);
                if (!table) {
                    gl::log(gl::LogLevel::warning,
                            "npc inventory: cannot load '" + person.inventory +
                                "': " + table.error().message);
                    cached = cache.emplace(person.inventory, std::nullopt).first;
                } else {
                    cached = cache.emplace(person.inventory, std::move(table.value())).first;
                }
            }
            if (cached->second) {
                auto generated = NpcLootGenerator::generate(
                    *cached->second, loot_level, names, item_template, rng, specials, force);
                if (!generated) {
                    ++counts.gate_failed;
                } else {
                    runtime.replace_person_trade_items(person.name,
                                                       convert_loot(generated->hot));
                    runtime.replace_person_items(person.name,
                                                 convert_loot(generated->main));
                    ++counts.populated;
                }
            } else {
                ++counts.missing_table;
            }
        }
        if (consume_init_draws)
            consume_person_init_rng(person.def, rng);
    }
    return counts;
}
} // namespace

void generate_npc_inventories(InventoryRuntime &runtime,
                              const std::vector<PersonInstance> &persons,
                              gl::assets::GameVfs &vfs, const gl::assets::ItemNameIndex &names,
                              InventoryRuntime::ItemTemplateLookup item_template,
                              std::int32_t loot_level, MsvcRandom &rng,
                              const gl::assets::SpecialDatabase *specials) {
    runtime.clear_persons();
    std::vector<std::size_t> indices(persons.size());
    for (std::size_t index = 0; index < indices.size(); ++index)
        indices[index] = index;
    generate_npc_indices(runtime, persons, indices, vfs, names, item_template,
                         loot_level, rng, specials, true, true);
}

NpcInventoryCounts regenerate_npc_inventories(
    InventoryRuntime &runtime, const std::vector<PersonInstance> &persons,
    const std::vector<std::size_t> &indices, gl::assets::GameVfs &vfs,
    const gl::assets::ItemNameIndex &names,
    InventoryRuntime::ItemTemplateLookup item_template, std::int32_t loot_level,
    MsvcRandom &rng, const gl::assets::SpecialDatabase *specials, bool force) {
    return generate_npc_indices(runtime, persons, indices, vfs, names,
                                item_template, loot_level, rng, specials, force, false);
}
} // namespace gl::game
