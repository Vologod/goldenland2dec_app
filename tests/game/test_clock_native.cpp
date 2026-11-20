#include "gl/game/calendar.hpp"
#include "gl/script/native_registry.hpp"

#include <doctest/doctest.h>
#include <span>
#include <vector>

using gl::game::GameDatetime;
using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::Value;

namespace {
GameDatetime make_date(std::uint32_t year, std::uint32_t month, std::uint32_t day,
                       std::uint32_t hour = 0) {
    GameDatetime value;
    value.year = year;
    value.month = month;
    value.day = day;
    value.weekday = 0;
    value.hour = hour;
    return value;
}

Value call(std::uint32_t id, NativeServices &services) {
    const std::vector<Value> args;
    const auto *info = NativeRegistry::find(id);
    REQUIRE(info != nullptr);
    return NativeRegistry::call(*info, args, services);
}
} // namespace

TEST_CASE("days from beginning matches the server formula") {
    CHECK(gl::game::days_from_beginning(make_date(1, 1, 1)) == 0);
    CHECK(gl::game::days_from_beginning(make_date(1, 1, 2)) == 1);
    CHECK(gl::game::days_from_beginning(make_date(1, 2, 1)) == 31);
    CHECK(gl::game::days_from_beginning(make_date(2, 1, 1)) >
          gl::game::days_from_beginning(make_date(1, 12, 31)));
    CHECK(gl::game::days_from_beginning(make_date(1125, 5, 17)) > 0);
}

TEST_CASE("day-night factor interpolates sunrise and sunset") {
    CHECK(gl::game::day_night_factor(make_date(1125, 5, 17, 6)) == doctest::Approx(0.0));
    CHECK(gl::game::day_night_factor(make_date(1125, 5, 17, 7)) == doctest::Approx(0.0));
    CHECK(gl::game::day_night_factor(make_date(1125, 5, 17, 8)) == doctest::Approx(1.0));
    CHECK(gl::game::day_night_factor(make_date(1125, 5, 17, 12)) == doctest::Approx(1.0));
    CHECK(gl::game::day_night_factor(make_date(1125, 5, 17, 21)) == doctest::Approx(1.0));
    CHECK(gl::game::day_night_factor(make_date(1125, 5, 17, 22)) == doctest::Approx(0.0));
}

TEST_CASE("day-night factor uses minutes and seconds") {
    GameDatetime sunrise = make_date(1125, 5, 17, 7);
    sunrise.minute = 30;
    CHECK(gl::game::day_night_factor(sunrise) == doctest::Approx(0.5));
    GameDatetime sunset = make_date(1125, 5, 17, 21);
    sunset.minute = 30;
    CHECK(gl::game::day_night_factor(sunset) == doctest::Approx(0.5));
}

TEST_CASE("clock natives return hour, day number and light") {
    GameDatetime now = make_date(1125, 5, 17, 13);
    NativeServices services;
    services.clock.current_hour = [&now] {
        return static_cast<std::int32_t>(now.hour);
    };
    services.clock.days_from_beginning = [&now] {
        return gl::game::days_from_beginning(now);
    };
    services.clock.day_night = [&now] {
        return gl::game::day_night_factor(now);
    };

    CHECK(call(0x6000001, services) == Value{double{13.0}});
    CHECK(call(0x6000002, services) ==
          Value{double{static_cast<double>(gl::game::days_from_beginning(now))}});
    CHECK(call(0x6000000, services) ==
          Value{double{gl::game::day_night_factor(now)}});
}

TEST_CASE("clock natives fail without a clock") {
    NativeServices services;
    CHECK(call(0x6000000, services) == Value{double{0.0}});
    CHECK(call(0x6000001, services) == Value{double{0.0}});
    CHECK(call(0x6000002, services) == Value{double{0.0}});
}

TEST_CASE("clock query natives are registered with server arities") {
    for (const std::uint32_t id : {0x6000000u, 0x6000001u, 0x6000002u}) {
        const auto *info = NativeRegistry::find(id);
        REQUIRE(info != nullptr);
        CHECK(info->support == gl::script::NativeSupport::supported);
        CHECK(info->arity == 0);
    }
}
