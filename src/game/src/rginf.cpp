#include "gl/game/rginf.hpp"

namespace gl::game {
namespace {
bool is_leap(std::uint32_t year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

std::uint32_t days_in_month(std::uint32_t year, std::uint32_t month) {
    switch (month) {
    case 4:
    case 6:
    case 9:
    case 11:
        return 30;
    case 2:
        return is_leap(year) ? 29 : 28;
    default:
        return 31;
    }
}

std::uint32_t u32_from_bytes(const std::array<std::uint8_t, 0x20> &bytes, std::size_t index) {
    const std::size_t offset = index * 4;
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}
} // namespace

GameDatetime GameDatetime::from_array(const std::array<std::uint32_t, 8> &value) {
    GameDatetime out;
    out.year = value[0];
    out.month = value[1];
    out.day = value[2];
    out.weekday = value[3];
    out.hour = value[4];
    out.minute = value[5];
    out.second = value[6];
    out.millisecond = value[7];
    return out;
}

GameDatetime GameDatetime::from_time_bytes(const std::array<std::uint8_t, 0x20> &value) {
    std::array<std::uint32_t, 8> fields{};
    for (std::size_t index = 0; index < fields.size(); ++index)
        fields[index] = u32_from_bytes(value, index);
    return from_array(fields);
}

std::array<std::uint32_t, 8> GameDatetime::to_array() const {
    return {year, month, day, weekday, hour, minute, second, millisecond};
}

bool datetime_le(const GameDatetime &lhs, const GameDatetime &rhs) {
    const std::array<std::uint32_t, 7> left{lhs.year, lhs.month, lhs.day, lhs.hour,
                                            lhs.minute, lhs.second, lhs.millisecond};
    const std::array<std::uint32_t, 7> right{rhs.year, rhs.month, rhs.day, rhs.hour,
                                             rhs.minute, rhs.second, rhs.millisecond};
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index] < right[index])
            return true;
        if (left[index] > right[index])
            return false;
    }
    return true;
}

GameDatetime add_days(const GameDatetime &value, std::int32_t days) {
    GameDatetime out = value;
    while (days > 0) {
        const std::uint32_t month_days = days_in_month(out.year, out.month);
        const std::int32_t remaining_in_month =
            static_cast<std::int32_t>(month_days - out.day);
        if (days <= remaining_in_month) {
            out.day += static_cast<std::uint32_t>(days);
            days = 0;
        } else {
            days -= remaining_in_month + 1;
            out.day = 1;
            ++out.month;
            if (out.month > 12) {
                out.month = 1;
                ++out.year;
            }
        }
    }
    return out;
}

RginfDecision decide_rginf(const gl::assets::SaveRginf &saved, const GameDatetime &now,
                           std::int32_t interval_days) {
    RginfDecision decision;
    const GameDatetime saved_ts = GameDatetime::from_array(saved.datetime);
    decision.due = saved.latch == 0 && datetime_le(add_days(saved_ts, interval_days), now);
    decision.runtime_latch = decision.due ? 1u : 0u;
    decision.timestamp = decision.due ? now : saved_ts;
    return decision;
}
} // namespace gl::game
