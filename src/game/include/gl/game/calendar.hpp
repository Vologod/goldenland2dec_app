#pragma once
#include "gl/game/rginf.hpp"

#include <cmath>
#include <cstdint>

namespace gl::game {
std::int64_t days_from_civil(std::int32_t year, std::uint32_t month, std::uint32_t day);
GameDatetime add_milliseconds(const GameDatetime &value, std::int64_t milliseconds);

class GameCalendar {
  public:
    GameCalendar() = default;
    explicit GameCalendar(GameDatetime value) : value_(value) {}

    const GameDatetime &now() const { return value_; }
    void set(GameDatetime value) { value_ = value; }
    void set_paused(bool paused) { paused_ = paused; }
    bool paused() const { return paused_; }
    void set_speed(double speed) {
        speed_ = (std::isfinite(speed) && speed >= 0.0) ? speed : 0.0;
    }
    double speed() const { return speed_; }
    void advance(std::uint64_t now_ms);

  private:
    GameDatetime value_;
    std::uint64_t last_ms_ = 0;
    bool has_last_ = false;
    bool paused_ = false;
    double speed_ = 1.0;
};
} // namespace gl::game
