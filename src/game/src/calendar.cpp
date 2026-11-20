#include "gl/game/calendar.hpp"

#include <cmath>

namespace gl::game {
namespace {
constexpr std::int64_t kMsPerDay = 86400000;

struct Civil {
    std::int32_t year = 0;
    std::uint32_t month = 0;
    std::uint32_t day = 0;
};

Civil civil_from_days(std::int64_t z) {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::uint32_t doe = static_cast<std::uint32_t>(z - era * 146097);
    const std::uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
    const std::uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::uint32_t mp = (5 * doy + 2) / 153;
    const std::uint32_t day = doy - (153 * mp + 2) / 5 + 1;
    const std::int64_t month = static_cast<std::int64_t>(mp) + (mp < 10 ? 3 : -9);
    return {static_cast<std::int32_t>(y + (month <= 2 ? 1 : 0)),
            static_cast<std::uint32_t>(month), day};
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    const std::int64_t q = a / b;
    const std::int64_t r = a % b;
    return r != 0 && ((r < 0) != (b < 0)) ? q - 1 : q;
}

std::int64_t floor_mod(std::int64_t a, std::int64_t b) {
    const std::int64_t r = a % b;
    return r != 0 && ((r < 0) != (b < 0)) ? r + b : r;
}
} // namespace

std::int64_t days_from_civil(std::int32_t year, std::uint32_t month, std::uint32_t day) {
    std::int64_t y = year;
    y -= month <= 2 ? 1 : 0;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const std::uint32_t yoe = static_cast<std::uint32_t>(y - era * 400);
    const std::int64_t m = static_cast<std::int64_t>(month);
    const std::uint32_t doy =
        static_cast<std::uint32_t>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1);
    const std::uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

GameDatetime add_milliseconds(const GameDatetime &value, std::int64_t milliseconds) {
    std::int64_t time_of_day = static_cast<std::int64_t>(value.hour) * 3600000 +
                               static_cast<std::int64_t>(value.minute) * 60000 +
                               static_cast<std::int64_t>(value.second) * 1000 +
                               static_cast<std::int64_t>(value.millisecond);
    const std::int64_t total = time_of_day + milliseconds;
    const std::int64_t day_carry = floor_div(total, kMsPerDay);
    time_of_day = floor_mod(total, kMsPerDay);

    GameDatetime out = value;
    out.millisecond = static_cast<std::uint32_t>(time_of_day % 1000);
    out.second = static_cast<std::uint32_t>((time_of_day / 1000) % 60);
    out.minute = static_cast<std::uint32_t>((time_of_day / 60000) % 60);
    out.hour = static_cast<std::uint32_t>(time_of_day / 3600000);

    const std::int64_t serial = days_from_civil(
        static_cast<std::int32_t>(value.year), value.month, value.day);
    const Civil civil = civil_from_days(serial + day_carry);
    out.year = static_cast<std::uint32_t>(civil.year);
    out.month = civil.month;
    out.day = civil.day;
    const std::int64_t weekday =
        (static_cast<std::int64_t>(value.weekday) + floor_mod(day_carry, 7) + 7) % 7;
    out.weekday = static_cast<std::uint32_t>(weekday);
    return out;
}

void GameCalendar::advance(std::uint64_t now_ms) {
    if (!has_last_) {
        last_ms_ = now_ms;
        has_last_ = true;
        return;
    }
    const std::uint32_t delta = static_cast<std::uint32_t>(now_ms - last_ms_);
    last_ms_ = now_ms;
    if (paused_)
        return;
    double scaled = static_cast<double>(delta) * 60.0 * speed_;
    if (!std::isfinite(scaled))
        return;
    if (scaled > 9.0e18)
        scaled = 9.0e18;
    const auto game_ms = std::llround(scaled);
    if (game_ms != 0)
        value_ = add_milliseconds(value_, static_cast<std::int64_t>(game_ms));
}
std::int32_t days_from_beginning(const GameDatetime &value) {
    if (value.year == 0) return 0;
    const std::int32_t year = static_cast<std::int32_t>(value.year);
    const std::int32_t previous = year - 1;
    std::int32_t offset = 0;
    switch (value.month) {
    case 12: offset += 30; [[fallthrough]];
    case 11: offset += 31; [[fallthrough]];
    case 10: offset += 30; [[fallthrough]];
    case 9: offset += 31; [[fallthrough]];
    case 8: offset += 31; [[fallthrough]];
    case 7: offset += 30; [[fallthrough]];
    case 6: offset += 31; [[fallthrough]];
    case 5: offset += 30; [[fallthrough]];
    case 4: offset += 31; [[fallthrough]];
    case 3:
        offset += (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
        [[fallthrough]];
    case 2: offset += 31; [[fallthrough]];
    default: break;
    }
    return (year * 73 - 73) * 5 +
           ((((previous >> 1) >> 30) - 1 + year) >> 2) - previous / 100 +
           previous / 400 - 1 + static_cast<std::int32_t>(value.day) + offset;
}
double day_night_factor(const GameDatetime &value) {
    const std::int32_t hour = static_cast<std::int32_t>(value.hour);
    if (hour < 7 || hour >= 22) return 0.0;
    if (hour >= 8 && hour < 21) return 1.0;
    if (hour == 12) return 1.0;
    const double minutes = static_cast<double>(value.minute) +
                           static_cast<double>(value.second) / 60.0;
    if (hour < 12) return minutes / 60.0;
    return 1.0 - minutes / 60.0;
}

} // namespace gl::game
