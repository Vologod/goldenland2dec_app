#include "gl/script/native_registry.hpp"
#include "gl/script/script_vm.hpp"

#include <doctest/doctest.h>
#include <span>
#include <string>
#include <vector>

namespace {
struct NullHost final : gl::script::ScriptHost {
    gl::script::Value call(std::uint32_t, std::span<const gl::script::Value>) override {
        return std::int32_t{0};
    }
};

gl::script::Value call(std::uint32_t id, std::initializer_list<gl::script::Value> args,
                       gl::script::NativeServices &services) {
    const std::vector<gl::script::Value> values(args);
    const auto *info = gl::script::NativeRegistry::find(id);
    REQUIRE(info != nullptr);
    return gl::script::NativeRegistry::call(*info, values, services);
}
} // namespace

TEST_CASE("set-location-access native writes the state global") {
    gl::script::NativeServices services;
    std::string key;
    double value = 0.0;
    int calls = 0;
    services.global.set_double = [&](std::string_view name, double state) {
        ++calls;
        key = std::string(name);
        value = state;
    };

    const auto *info = gl::script::NativeRegistry::find(0x7000008);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 2);
    CHECK(info->handler != nullptr);

    CHECK(call(0x7000008, {std::string{"L96"}, std::int32_t{2}}, services) ==
          gl::script::Value{double{0.0}});
    CHECK(calls == 1);
    CHECK(key == "L96_state");
    CHECK(value == doctest::Approx(2.0));

    CHECK(call(0x7000008, {std::string{"L96"}, double{3.0}}, services) ==
          gl::script::Value{double{0.0}});
    CHECK(calls == 1);
    CHECK(call(0x7000008, {std::string{"L96"}, std::int32_t{3}}, services) ==
          gl::script::Value{double{0.0}});
    CHECK(calls == 2);
    CHECK(value == doctest::Approx(3.0));

    CHECK(call(0x7000008, {std::string{"L96"}, std::int32_t{-1}}, services) ==
          gl::script::Value{double{0.0}});
    CHECK(calls == 3);
    CHECK(value == doctest::Approx(4294967295.0));

    CHECK(call(0x7000008, {std::string{"L96"}, std::string{"2"}}, services) ==
          gl::script::Value{double{0.0}});
    CHECK(calls == 3);
    CHECK(call(0x7000008, {std::int32_t{1}, std::int32_t{2}}, services) ==
          gl::script::Value{double{0.0}});
    CHECK(calls == 3);
}

TEST_CASE("location access updates a declared global only") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    gl::script::ScriptGlobal declared;
    declared.name = "L52_1_state";
    declared.value = std::int32_t{1};
    vm.set_globals(std::span<const gl::script::ScriptGlobal>(&declared, 1));

    gl::script::NativeServices services;
    services.global.set_double = [&](std::string_view name, double value) {
        vm.set_global(name, gl::script::Value{double{value}});
    };
    CHECK(call(0x7000008, {std::string{"L52_1"}, std::int32_t{2}}, services) ==
          gl::script::Value{double{0.0}});
    REQUIRE(vm.globals().count("L52_1_state") == 1);
    CHECK(std::get<double>(vm.globals().at("L52_1_state")) == doctest::Approx(2.0));

    CHECK(call(0x7000008, {std::string{"L96"}, std::int32_t{2}}, services) ==
          gl::script::Value{double{0.0}});
    CHECK(vm.globals().count("L96_state") == 0);
}

TEST_CASE("set-location-access is a no-op without a service") {
    gl::script::NativeServices services;
    CHECK(call(0x7000008, {std::string{"L96"}, std::int32_t{2}}, services) ==
          gl::script::Value{double{0.0}});
}
