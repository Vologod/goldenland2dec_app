#pragma once

namespace gl::app {

enum class PendingTalkDecision { wait, open, retarget, abandon };

inline PendingTalkDecision resolve_pending_talk(bool can_interact, bool hero_has_goal,
                                                bool npc_moved = false) {
    if (can_interact) return PendingTalkDecision::open;
    if (npc_moved) return PendingTalkDecision::retarget;
    if (!hero_has_goal) return PendingTalkDecision::abandon;
    return PendingTalkDecision::wait;
}

} // namespace gl::app
