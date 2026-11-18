#pragma once
#include "gl/assets/hero_equipment.hpp"
#include "gl/assets/save_item.hpp"

namespace gl::assets {
InventoryItem inventory_item_from_save(const SaveItemRecord &record);
SaveItemRecord save_item_from_inventory(const InventoryItem &item);
} // namespace gl::assets
