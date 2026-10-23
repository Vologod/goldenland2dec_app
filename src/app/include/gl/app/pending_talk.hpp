#pragma once

namespace gl::app {

enum class PendingTalkDecision { wait, open, abandon };

inline PendingTalkDecision resolve_pending_talk(bool can_interact, bool hero_has_goal) {
    if (can_interact) return PendingTalkDecision::open;
    if (!hero_has_goal) return PendingTalkDecision::abandon;
    return PendingTalkDecision::wait;
}

} // namespace gl::app
