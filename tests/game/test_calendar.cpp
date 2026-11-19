#include "gl/game/calendar.hpp"

#include <array>
#include <cstddef>
#include <doctest/doctest.h>
#include <limits>

using gl::game::GameCalendar;
using gl::game::GameDatetime;

namespace {
GameDatetime dt(std::uint32_t year, std::uint32_t month, std::uint32_t day,
                std::uint32_t hour = 0, std::uint32_t minute = 0,
                std::uint32_t second = 0, std::uint32_t ms = 0,
                std::uint32_t weekday = 0) {
    GameDatetime value;
    value.year = year;
    value.month = month;
    value.day = day;
    value.weekday = weekday;
    value.hour = hour;
    value.minute = minute;
    value.second = second;
    value.millisecond = ms;
    return value;
}
} // namespace

TEST_CASE("days_from_civil follows the Gregorian calendar") {
    CHECK(gl::game::days_from_civil(1970, 1, 1) == 0);
    CHECK(gl::game::days_from_civil(1970, 1, 2) == 1);
    CHECK(gl::game::days_from_civil(1969, 12, 31) == -1);
    CHECK(gl::game::days_from_civil(2000, 3, 1) -
              gl::game::days_from_civil(2000, 2, 29) == 1);
    CHECK(gl::game::days_from_civil(1900, 3, 1) -
              gl::game::days_from_civil(1900, 2, 28) == 1);
    CHECK(gl::game::days_from_civil(2001, 1, 1) -
              gl::game::days_from_civil(2000, 1, 1) == 366);
}

TEST_CASE("add_milliseconds carries time and Gregorian dates") {
    const auto base = dt(2000, 2, 28, 23, 59, 59, 999, 6);
    const auto next = gl::game::add_milliseconds(base, 1);
    CHECK(next.year == 2000);
    CHECK(next.month == 2);
    CHECK(next.day == 29);
    CHECK(next.hour == 0);
    CHECK(next.minute == 0);
    CHECK(next.second == 0);
    CHECK(next.millisecond == 0);
    CHECK(next.weekday == 0);

    const auto leap = gl::game::add_milliseconds(dt(2000, 2, 28), 86400000);
    CHECK(leap.month == 2);
    CHECK(leap.day == 29);
    const auto non_leap = gl::game::add_milliseconds(dt(1900, 2, 28), 86400000);
    CHECK(non_leap.month == 3);
    CHECK(non_leap.day == 1);

    const auto time = gl::game::add_milliseconds(dt(2020, 1, 1), 3723004);
    CHECK(time.hour == 1);
    CHECK(time.minute == 2);
    CHECK(time.second == 3);
    CHECK(time.millisecond == 4);

    const auto second = gl::game::add_milliseconds(dt(2020, 1, 1), 1000);
    CHECK(second.second == 1);
    const auto year_end = gl::game::add_milliseconds(dt(2020, 12, 31), 86400000);
    CHECK(year_end.year == 2021);
    CHECK(year_end.month == 1);
    CHECK(year_end.day == 1);
}

TEST_CASE("GameDatetime save array round-trips") {
    const auto value = dt(2020, 2, 29, 23, 59, 58, 997, 6);
    CHECK(GameDatetime::from_array(value.to_array()) == value);
    CHECK(GameDatetime::from_time_bytes(
              [] {
                  std::array<std::uint8_t, 0x20> bytes{};
                  const auto fields = dt(2020, 2, 29, 23, 59, 58, 997, 6).to_array();
                  for (std::size_t i = 0; i < fields.size(); ++i)
                      for (std::size_t b = 0; b < 4; ++b)
                          bytes[i * 4 + b] =
                              static_cast<std::uint8_t>((fields[i] >> (8 * b)) & 0xFF);
                  return bytes;
              }()) == value);
}

TEST_CASE("add_milliseconds wraps the weekday by day carry") {
    CHECK(gl::game::add_milliseconds(dt(2020, 1, 1, 0, 0, 0, 0, 6), 86400000).weekday == 0);
    CHECK(gl::game::add_milliseconds(dt(2020, 1, 1, 0, 0, 0, 0, 0), -86400000).weekday == 6);
}

TEST_CASE("game calendar advances at 60x real time") {
    GameCalendar calendar(dt(2020, 1, 1, 0, 0, 0, 0, 0));
    calendar.advance(0);
    calendar.advance(1000);
    CHECK(calendar.now().minute == 1);
    CHECK(calendar.now().hour == 0);
    calendar.advance(1000 + 24 * 60 * 1000);
    CHECK(calendar.now().day == 2);
    CHECK(calendar.now().hour == 0);
    CHECK(calendar.now().weekday == 1);
}

TEST_CASE("game calendar handles pause and speed") {
    GameCalendar calendar(dt(2020, 1, 1));
    calendar.advance(0);
    calendar.set_paused(true);
    calendar.advance(5000);
    CHECK(calendar.now().minute == 0);
    calendar.set_paused(false);
    calendar.set_speed(0.5);
    calendar.advance(6000);
    CHECK(calendar.now().second == 30);
    calendar.set_speed(2.0);
    calendar.advance(7000);
    CHECK(calendar.now().minute == 2);
    CHECK(calendar.now().second == 30);
    calendar.set_speed(-1.0);
    CHECK(calendar.speed() == 0.0);
    calendar.set_speed(std::numeric_limits<double>::infinity());
    CHECK(calendar.speed() == 0.0);
}

TEST_CASE("game calendar uses a wrapping delta") {
    GameCalendar calendar(dt(2020, 1, 1));
    calendar.advance(0xFFFFFF00u);
    calendar.advance(1000);
    CHECK(calendar.now().minute == 1);
}
