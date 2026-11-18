#include "gl/assets/save_item_convert.hpp"

namespace gl::assets {
InventoryItem inventory_item_from_save(const SaveItemRecord &record) {
    InventoryItem item;
    item.item_id = record.item_id;
    item.count = record.count;
    item.flags = record.flags;
    item.type = record.type;
    item.style = record.style;
    item.durability = record.durability;
    item.price = record.price;
    item.weight_x10 = record.weight_x10;
    item.unknown_a = record.unknown_a;
    item.material = record.material;
    item.mod_count = record.mod_count;
    item.mods.reserve(record.modifiers.size());
    for (const SaveItemModifier &modifier : record.modifiers) {
        SpecialMod mod;
        mod.id = modifier.words[0];
        mod.value = static_cast<std::int32_t>(modifier.words[1]);
        mod.flags = modifier.words[2];
        mod.word3 = modifier.words[3];
        mod.word4 = modifier.words[4];
        item.mods.push_back(mod);
    }
    item.save_record = record;
    return item;
}

SaveItemRecord save_item_from_inventory(const InventoryItem &item) {
    SaveItemRecord record = item.save_record.value_or(SaveItemRecord{});
    record.item_id = item.item_id;
    record.count = item.count;
    record.flags = item.flags;
    record.type = item.type;
    record.style = item.style;
    record.durability = item.durability;
    record.price = item.price;
    record.weight_x10 = item.weight_x10;
    record.unknown_a = item.unknown_a;
    record.material = item.material;
    record.mod_count = static_cast<std::uint32_t>(item.mods.size());
    record.modifiers.clear();
    record.modifiers.reserve(item.mods.size());
    for (const SpecialMod &mod : item.mods)
        record.modifiers.push_back(
            {{{mod.id, static_cast<std::uint32_t>(mod.value), mod.flags, mod.word3, mod.word4}}});
    return record;
}
} // namespace gl::assets
