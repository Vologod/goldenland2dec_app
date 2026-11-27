#pragma once
#include "gl/assets/hero_stats.hpp"
#include "gl/assets/itm.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/person_runtime.hpp"

#include <cstddef>

namespace gl::game {
bool is_consumable(gl::assets::ItemType type);

struct HeroUseResult {
    bool used = false;
    bool consumed = false;
    int effects_added = 0;
};

HeroUseResult use_hero_item(const gl::assets::Item &item, HeroItemContainer container,
                            std::size_t index, gl::assets::HeroStats &stats,
                            PersonRuntime &runtime, InventoryRuntime &inventory);
} // namespace gl::game
