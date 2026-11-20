#include "gl/game/time_event_runtime.hpp"
#include "gl/game/calendar.hpp"

#include <utility>

namespace gl::game {
void TimeEventRuntime::set_now(Now now) { now_ = std::move(now); }

std::int32_t TimeEventRuntime::set_event(std::int32_t days, std::int32_t hours) {
    const GameDatetime current = now_ ? now_() : GameDatetime{};
    const std::int32_t handle = next_handle_++;
    const std::int64_t offset = static_cast<std::int64_t>(days) * 86400000 +
                                static_cast<std::int64_t>(hours) * 3600000;
    const GameDatetime due = add_milliseconds(current, offset);
    events_[handle] = TimeEvent{due};
    return handle;
}

std::int32_t TimeEventRuntime::get_event(std::int32_t handle) const {
    const auto it = events_.find(handle);
    if (it == events_.end() || !now_) return 0;
    return datetime_le(it->second.due, now_()) ? 1 : 0;
}

void TimeEventRuntime::clear_event(std::int32_t handle) {
    events_.erase(handle);
    if (events_.empty()) next_handle_ = 1;
}

void TimeEventRuntime::clear() {
    next_handle_ = 1;
    events_.clear();
}

gl::script::TimeEventAccess TimeEventRuntime::access() {
    gl::script::TimeEventAccess access;
    access.set_event = [this](std::int32_t days, std::int32_t hours) {
        return set_event(days, hours);
    };
    access.get_event = [this](std::int32_t handle) { return get_event(handle); };
    access.clear_event = [this](std::int32_t handle) { clear_event(handle); };
    return access;
}
} // namespace gl::game
