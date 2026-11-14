#pragma once
#include "gl/assets/save_payload.hpp"
#include "gl/game/level.hpp"
#include <cstddef>

namespace gl::game {
struct RestoreCounts {
    std::size_t matched = 0;
    std::size_t sef_only = 0;
    std::size_t type2_only = 0;
    std::size_t skipped = 0;
};

RestoreCounts restore_save_persons(Level &level, const gl::assets::SaveTailEntry &tail,
                                   const gl::assets::SaveTribeTable *tribes,
                                   bool drop_sef_only = false);
} // namespace gl::game
