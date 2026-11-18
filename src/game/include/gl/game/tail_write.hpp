#pragma once
#include "gl/assets/save_payload.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/save_restore.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace gl::game {
struct TailPersonLive {
    bool alive = true;
    std::optional<std::int32_t> hp;
    std::optional<std::int32_t> energy;
    std::optional<std::int32_t> ap;
};

Result<gl::assets::SaveTailEntry> build_selected_tail(
    const gl::assets::SaveTailEntry &loaded, const Level &level,
    const LevelSimulation &simulation, const InventoryRuntime &inventory,
    const RestoreResult &restore, std::span<const TailPersonLive> live,
    std::span<const std::size_t> new_persons, const gl::assets::SaveRginf &rginf,
    const gl::assets::SaveTribeTable *tribes);
} // namespace gl::game
