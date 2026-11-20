#include "gl/game/weather_runtime.hpp"
#include "gl/script/native_registry.hpp"

#include <doctest/doctest.h>
#include <span>
#include <vector>

using gl::game::WeatherRuntime;
using gl::script::NativeRegistry;
using gl::script::NativeServices;
using gl::script::Value;

namespace {
Value call(std::uint32_t id, std::initializer_list<Value> args, NativeServices &services) {
    const std::vector<Value> values(args);
    const auto *info = NativeRegistry::find(id);
    REQUIRE(info != nullptr);
    return NativeRegistry::call(*info, values, services);
}
} // namespace

TEST_CASE("weather runtime maps script codes") {
    WeatherRuntime runtime;
    CHECK(runtime.current() == 0);
    runtime.set_from_script(1);
    CHECK(runtime.current() == 0x10001);
    runtime.set_from_script(2);
    CHECK(runtime.current() == 0x10002);
    runtime.set_from_script(3);
    CHECK(runtime.current() == 0);
    runtime.set_from_script(-1);
    CHECK(runtime.current() == 0);
    runtime.clear();
    CHECK(runtime.current() == 0);
}

TEST_CASE("set-weather native dispatches numeric literals") {
    WeatherRuntime runtime;
    NativeServices services;
    services.weather = runtime.access();

    CHECK(call(0x700000b, {double{1.0}}, services) == Value{double{0.0}});
    CHECK(runtime.current() == 0x10001);
    CHECK(call(0x700000b, {std::int32_t{2}}, services) == Value{double{0.0}});
    CHECK(runtime.current() == 0x10002);
    CHECK(call(0x700000b, {std::string{"1"}}, services) == Value{double{0.0}});
    CHECK(runtime.current() == 0x10002);
}

TEST_CASE("set-weather native fails without a runtime") {
    NativeServices services;
    CHECK(call(0x700000b, {std::int32_t{1}}, services) == Value{double{0.0}});
}

TEST_CASE("set-weather native is registered with the server arity") {
    const auto *info = NativeRegistry::find(0x700000b);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 1);
}
