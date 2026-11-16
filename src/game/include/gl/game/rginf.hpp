#pragma once
#include "gl/assets/save_payload.hpp"
#include <array>
#include <cstdint>

namespace gl::game {
struct GameDatetime {
    std::uint32_t year = 0;
    std::uint32_t month = 0;
    std::uint32_t day = 0;
    std::uint32_t weekday = 0;
    std::uint32_t hour = 0;
    std::uint32_t minute = 0;
    std::uint32_t second = 0;
    std::uint32_t millisecond = 0;

    static GameDatetime from_array(const std::array<std::uint32_t, 8> &value);
    static GameDatetime from_time_bytes(const std::array<std::uint8_t, 0x20> &value);
    std::array<std::uint32_t, 8> to_array() const;
};

inline bool operator==(const GameDatetime &lhs, const GameDatetime &rhs) {
    return lhs.year == rhs.year && lhs.month == rhs.month && lhs.day == rhs.day &&
           lhs.weekday == rhs.weekday && lhs.hour == rhs.hour &&
           lhs.minute == rhs.minute && lhs.second == rhs.second &&
           lhs.millisecond == rhs.millisecond;
}

struct RginfDecision {
    std::uint32_t runtime_latch = 0;
    GameDatetime timestamp;
    bool due = false;
};

constexpr std::int32_t kSinglePlayerItemsRegenerateIntervalDays = 7;

bool datetime_le(const GameDatetime &lhs, const GameDatetime &rhs);
GameDatetime add_days(const GameDatetime &value, std::int32_t days);
RginfDecision decide_rginf(const gl::assets::SaveRginf &saved, const GameDatetime &now,
                           std::int32_t interval_days);
} // namespace gl::game
