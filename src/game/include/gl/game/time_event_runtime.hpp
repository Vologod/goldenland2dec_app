#pragma once
#include "gl/game/rginf.hpp"
#include "gl/script/time_event_access.hpp"

#include <cstdint>
#include <functional>
#include <map>

namespace gl::game {
struct TimeEvent {
    GameDatetime due;
};

class TimeEventRuntime {
  public:
    using Now = std::function<GameDatetime()>;

    void set_now(Now now);
    std::int32_t set_event(std::int32_t days, std::int32_t hours);
    std::int32_t get_event(std::int32_t handle) const;
    void clear_event(std::int32_t handle);
    void clear();

    gl::script::TimeEventAccess access();

  private:
    Now now_;
    std::int32_t next_handle_ = 1;
    std::map<std::int32_t, TimeEvent> events_;
};
} // namespace gl::game
