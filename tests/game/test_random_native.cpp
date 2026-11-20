#include "gl/game/msvc_random.hpp"
#include "gl/script/native_registry.hpp"

#include <doctest/doctest.h>
#include <span>
#include <vector>

using gl::game::MsvcRandom;
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

TEST_CASE("rand-min-max uses the shared MsvcRandom stream") {
    MsvcRandom expected_rng(7);
    MsvcRandom actual_rng(7);
    NativeServices services;
    services.random.range = [&actual_rng](std::int32_t low, std::int32_t high) {
        return actual_rng.range(low, high);
    };

    for (int index = 0; index < 20; ++index) {
        const std::int32_t expected = expected_rng.range(3, 9);
        const Value result = call(0x700000a, {double{3.0}, double{9.0}}, services);
        REQUIRE(std::holds_alternative<double>(result));
        CHECK(std::get<double>(result) == static_cast<double>(expected));
    }
}

TEST_CASE("rand-min-max keeps the inverted range behavior") {
    MsvcRandom rng(1);
    NativeServices services;
    services.random.range = [&rng](std::int32_t low, std::int32_t high) {
        return rng.range(low, high);
    };
    CHECK(call(0x700000a, {double{9.0}, double{3.0}}, services) ==
          Value{double{9.0}});
}

TEST_CASE("rand-min-max rejects strings and missing runtime") {
    NativeServices services;
    CHECK(call(0x700000a, {std::int32_t{1}, std::int32_t{2}}, services) ==
          Value{double{0.0}});

    MsvcRandom rng(1);
    services.random.range = [&rng](std::int32_t low, std::int32_t high) {
        return rng.range(low, high);
    };
    CHECK(call(0x700000a, {std::string{"a"}, std::int32_t{2}}, services) ==
          Value{double{0.0}});
    MsvcRandom literal_rng(1);
    services.random.range = [&literal_rng](std::int32_t low, std::int32_t high) {
        return literal_rng.range(low, high);
    };
    CHECK(std::holds_alternative<double>(
        call(0x700000a, {double{1.0}, double{2.0}}, services)));
}

TEST_CASE("rand-min-max is registered with the server arity") {
    const auto *info = NativeRegistry::find(0x700000a);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 2);
}
