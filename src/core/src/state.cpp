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
    switch (state_) {
    case AppState::boot:
        allowed = next == AppState::menu;
        break;
    case AppState::menu:
        allowed = next == AppState::level;
        break;
    case AppState::level:
        allowed = next == AppState::menu || next == AppState::level;
        break;
    case AppState::exit:
        break;
    }

    if (allowed)
        state_ = next;
    return allowed;
}
} // namespace gl::core
