#include "gl/game/time_event_runtime.hpp"
#include "gl/script/native_registry.hpp"

#include <doctest/doctest.h>
#include <span>
#include <vector>

using gl::game::GameDatetime;
using gl::game::TimeEventRuntime;
using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::Value;

namespace {
GameDatetime at(std::uint32_t day, std::uint32_t hour) {
    GameDatetime value;
    value.year = 1125;
    value.month = 5;
    value.day = day;
    value.weekday = 4;
    value.hour = hour;
    return value;
}

Value call(std::uint32_t id, std::initializer_list<Value> args, NativeServices &services) {
    const std::vector<Value> values(args);
    const auto *info = NativeRegistry::find(id);
    REQUIRE(info != nullptr);
    return NativeRegistry::call(*info, values, services);
}
} // namespace

TEST_CASE("time events add days and hours to the current time") {
    GameDatetime now = at(17, 12);
    TimeEventRuntime runtime;
    runtime.set_now([&now] { return now; });

    const std::int32_t handle = runtime.set_event(1, 6);
    CHECK(handle == 1);
    CHECK(runtime.get_event(handle) == 0);
    now = at(18, 17);
    CHECK(runtime.get_event(handle) == 0);
    now = at(18, 18);
    CHECK(runtime.get_event(handle) == 1);
    now = at(19, 0);
    CHECK(runtime.get_event(handle) == 1);
}

TEST_CASE("time event negative days combine with positive hours") {
    GameDatetime now = at(17, 12);
    TimeEventRuntime runtime;
    runtime.set_now([&now] { return now; });
    const std::int32_t handle = runtime.set_event(-1, 48);
    now = at(18, 11);
    CHECK(runtime.get_event(handle) == 0);
    now = at(18, 12);
    CHECK(runtime.get_event(handle) == 1);
}

TEST_CASE("time event zero offset is due immediately") {
    GameDatetime now = at(17, 12);
    TimeEventRuntime runtime;
    runtime.set_now([&now] { return now; });
    const std::int32_t handle = runtime.set_event(0, 0);
    CHECK(runtime.get_event(handle) == 1);
}

TEST_CASE("time event handles reset when the list becomes empty") {
    GameDatetime now = at(17, 12);
    TimeEventRuntime runtime;
    runtime.set_now([&now] { return now; });

    const std::int32_t first = runtime.set_event(0, 0);
    const std::int32_t second = runtime.set_event(0, 1);
    CHECK(second == first + 1);
    runtime.clear_event(second);
    CHECK(runtime.get_event(second) == 0);
    runtime.clear_event(first);
    CHECK(runtime.set_event(0, 2) == 1);
    runtime.clear();
    CHECK(runtime.get_event(1) == 0);
    CHECK(runtime.set_event(0, 2) == 1);
    runtime.clear_event(999);
    CHECK(runtime.get_event(999) == 0);
}

TEST_CASE("time event natives dispatch and keep the clear return") {
    GameDatetime now = at(17, 12);
    TimeEventRuntime runtime;
    runtime.set_now([&now] { return now; });
    NativeServices services;
    services.time_event = runtime.access();

    const Value handle = call(0x7000005, {std::int32_t{0}, std::int32_t{2}}, services);
    REQUIRE(std::holds_alternative<double>(handle));
    const std::int32_t value = static_cast<std::int32_t>(std::get<double>(handle));
    CHECK(value == 1);
    CHECK(call(0x7000006, {std::int32_t{1}}, services) == Value{std::int32_t{0}});
    now = at(17, 14);
    CHECK(call(0x7000006, {std::int32_t{1}}, services) == Value{std::int32_t{1}});
    CHECK(call(0x7000007, {std::int32_t{1}}, services) == Value{double{0.0}});
    CHECK(call(0x7000006, {std::int32_t{1}}, services) == Value{std::int32_t{0}});
}

TEST_CASE("time event natives reject strings and missing runtime") {
    NativeServices services;
    CHECK(call(0x7000005, {std::int32_t{1}, std::int32_t{1}}, services) ==
          Value{double{0.0}});
    CHECK(call(0x7000006, {std::int32_t{1}}, services) == Value{double{0.0}});
    CHECK(call(0x7000007, {std::int32_t{1}}, services) == Value{double{0.0}});

    GameDatetime now = at(17, 12);
    TimeEventRuntime runtime;
    runtime.set_now([&now] { return now; });
    services.time_event = runtime.access();
    CHECK(call(0x7000005, {std::string{"a"}, std::int32_t{1}}, services) ==
          Value{double{0.0}});
    CHECK(call(0x7000005, {std::int32_t{1}, std::string{"a"}}, services) ==
          Value{double{0.0}});
    CHECK(call(0x7000006, {std::string{"a"}}, services) == Value{double{0.0}});
    CHECK(call(0x7000007, {std::string{"a"}}, services) == Value{double{0.0}});
    CHECK(runtime.set_event(0, 1) == 1);
    CHECK(runtime.get_event(1) == 0);
}

TEST_CASE("time event natives are registered with server arities") {
    const std::pair<std::uint32_t, std::uint8_t> expected[] = {
        {0x7000005, 2}, {0x7000006, 1}, {0x7000007, 1}};
    for (const auto &[id, arity] : expected) {
        const auto *info = NativeRegistry::find(id);
        REQUIRE(info != nullptr);
        CHECK(info->support == gl::script::NativeSupport::supported);
        CHECK(info->arity == arity);
    }
}
