#include "gl/core/state.hpp"

namespace gl::core {
AppState StateMachine::state() const { return state_; }

bool StateMachine::transition(AppState next) {
    if (state_ == AppState::exit)
        return false;

    if (next == AppState::exit) {
        state_ = AppState::exit;
        return true;
    }

    bool allowed = false;
    if (state_ == AppState::boot) {
        allowed = next == AppState::menu;
    } else if (state_ == AppState::menu) {
        allowed = next == AppState::level;
    } else if (state_ == AppState::level) {
        allowed = next == AppState::menu || next == AppState::level;
    }

    if (allowed)
        state_ = next;
    return allowed;
}
} // namespace gl::core
