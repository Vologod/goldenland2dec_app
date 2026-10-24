#pragma once

#include <gl/game/level_simulation.hpp>
#include <gl/game/pathfinder.hpp>

namespace gl::app {

enum class PersonClickAction { ignore, open, approach };

inline PersonClickAction resolve_person_click(bool talkable, bool can_interact) {
    if (!talkable) return PersonClickAction::ignore;
    if (can_interact) return PersonClickAction::open;
    return PersonClickAction::approach;
}

inline gl::game::Tile current_person_tile(const gl::game::PersonMovement& movement) {
    return gl::game::tile_at(movement.x, movement.y);
}

} // namespace gl::app
