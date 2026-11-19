#pragma once
#include "gl/assets/save_payload.hpp"
#include "gl/game/calendar.hpp"
#include "gl/game/hero_save_write.hpp"
#include "gl/game/inventory_runtime.hpp"
#include "gl/game/level.hpp"
#include "gl/game/level_simulation.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/game/save_restore.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace gl::game {
struct LiveSaveInput {
    const Level *level = nullptr;
    const LevelSimulation *simulation = nullptr;
    const InventoryRuntime *inventory = nullptr;
    const RestoreResult *restore = nullptr;
    const PersonRuntime *persons = nullptr;
    const HeroLiveState *hero = nullptr;
    std::uint32_t rginf_latch = 0;
    bool has_rginf_latch = false;
    const GameCalendar *calendar = nullptr;
    std::optional<GameDatetime> rginf_datetime;
    std::vector<gl::assets::SaveVar> vars;
};

Result<gl::assets::SavePayload> apply_live_save(gl::assets::SavePayload payload,
                                                const LiveSaveInput &input);
} // namespace gl::game
