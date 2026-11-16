#include "gl/game/rginf.hpp"

#include <array>
#include <cstdint>
#include <doctest/doctest.h>

namespace {
gl::game::GameDatetime dt(std::uint32_t year, std::uint32_t month, std::uint32_t day,
                          std::uint32_t hour = 0, std::uint32_t minute = 0,
                          std::uint32_t second = 0, std::uint32_t millisecond = 0,
                          std::uint32_t weekday = 0) {
    gl::game::GameDatetime value;
    value.year = year;
    value.month = month;
    value.day = day;
    value.weekday = weekday;
    value.hour = hour;
    value.minute = minute;
    value.second = second;
    value.millisecond = millisecond;
    return value;
}
} // namespace

TEST_CASE("rginf decision follows the saved latch and due check") {
    const gl::game::GameDatetime now = dt(1126, 6, 6, 20, 29, 25, 120);
    gl::assets::SaveRginf saved;
    saved.datetime = dt(1126, 5, 30, 16, 17, 25, 120).to_array();

    saved.latch = 0;
    const auto due = gl::game::decide_rginf(saved, now, 7);
    CHECK(due.due);
    CHECK(due.runtime_latch == 1);
    CHECK(due.timestamp.year == now.year);
    CHECK(due.timestamp.day == now.day);

    const gl::game::GameDatetime soon = dt(1126, 5, 30, 16, 20, 0, 0);
    const auto not_due = gl::game::decide_rginf(saved, soon, 7);
    CHECK_FALSE(not_due.due);
    CHECK(not_due.runtime_latch == 0);
    CHECK(not_due.timestamp.day == saved.datetime[2]);

    saved.latch = 1;
    const auto latched = gl::game::decide_rginf(saved, now, 7);
    CHECK_FALSE(latched.due);
    CHECK(latched.runtime_latch == 0);
    CHECK(latched.timestamp.day == saved.datetime[2]);
}

TEST_CASE("rginf datetime comparison ignores weekday and treats equality as due") {
    const auto a = dt(1124, 12, 16, 6, 58, 51, 660, 1);
    const auto same = dt(1124, 12, 16, 6, 58, 51, 660, 5);
    const auto later_ms = dt(1124, 12, 16, 6, 58, 51, 661, 1);
    const auto later_hour = dt(1124, 12, 16, 7, 58, 51, 660, 1);
    CHECK(gl::game::datetime_le(a, same));
    CHECK(gl::game::datetime_le(a, later_ms));
    CHECK(gl::game::datetime_le(a, later_hour));
    CHECK_FALSE(gl::game::datetime_le(later_ms, a));
    CHECK_FALSE(gl::game::datetime_le(later_hour, a));
}

TEST_CASE("rginf add_days crosses month, leap and year boundaries") {
    CHECK(gl::game::add_days(dt(1124, 5, 10, 1, 2, 3, 4), 7) ==
          dt(1124, 5, 17, 1, 2, 3, 4));
    CHECK(gl::game::add_days(dt(1124, 2, 29, 1, 2, 3, 4), 7) ==
          dt(1124, 3, 7, 1, 2, 3, 4));
    CHECK(gl::game::add_days(dt(1124, 12, 31, 1, 2, 3, 4), 7) ==
          dt(1125, 1, 7, 1, 2, 3, 4));
    CHECK(gl::game::add_days(dt(1125, 1, 31, 0, 0, 0, 0), 1) ==
          dt(1125, 2, 1, 0, 0, 0, 0));
}

TEST_CASE("rginf time bytes round-trip") {
    std::array<std::uint8_t, 0x20> bytes{};
    const std::array<std::uint32_t, 8> fields{1126, 5, 30, 2, 16, 17, 25, 120};
    for (std::size_t index = 0; index < fields.size(); ++index)
        for (std::size_t byte = 0; byte < 4; ++byte)
            bytes[index * 4 + byte] =
                static_cast<std::uint8_t>((fields[index] >> (byte * 8)) & 0xFF);
    const auto value = gl::game::GameDatetime::from_time_bytes(bytes);
    CHECK(value.to_array() == fields);
}
