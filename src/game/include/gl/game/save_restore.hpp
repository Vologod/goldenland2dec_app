#pragma once
#include "gl/assets/save_payload.hpp"
#include "gl/game/level.hpp"
#include "gl/game/pathfinder.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gl::game {
class InventoryRuntime;

struct RestoreCounts {
    std::size_t matched = 0;
    std::size_t sef_only = 0;
    std::size_t type2_only = 0;
    std::size_t skipped = 0;
    std::size_t containers_applied = 0;
    std::size_t duplicate_containers = 0;
    std::size_t c2_items = 0;
};

struct RestoredPerson {
    std::size_t level_index = 0;
    Tile current;
    std::uint8_t direction = 0;
    std::int32_t route_index = 0;
    bool loot_regenerate = false;
};

struct RestoreResult {
    RestoreCounts counts;
    std::vector<RestoredPerson> persons;
};

RestoreResult restore_save_persons(Level &level, const gl::assets::SaveTailEntry &tail,
                                   const gl::assets::SaveTribeTable *tribes,
                                   bool drop_sef_only = false,
                                   InventoryRuntime *inventory = nullptr);
} // namespace gl::game
