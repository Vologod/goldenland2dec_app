#pragma once

namespace gl::core {
enum class AppState { boot, menu, level, exit };

class StateMachine {
  public:
    AppState state() const;
    bool transition(AppState next);

  private:
    AppState state_ = AppState::boot;
};
} // namespace gl::core
