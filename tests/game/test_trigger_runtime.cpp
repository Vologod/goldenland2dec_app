#include "gl/assets/vfs.hpp"
#include "gl/game/level.hpp"
#include "gl/game/trigger_runtime.hpp"
#include "gl/script/native_registry.hpp"

#include <cstdlib>
#include <doctest/doctest.h>
#include <span>
#include <string>
#include <vector>

using gl::game::TriggerRuntime;
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

TEST_CASE("trigger runtime resets and toggles known names") {
    gl::assets::LvlDescription first;
    first.name = "enter_house";
    gl::assets::LvlDescription second;
    second.name = "open_gate";
    const std::vector<gl::assets::LvlDescription> descriptions{first, second};

    TriggerRuntime runtime;
    runtime.reset(descriptions);
    CHECK(runtime.size() == 2);
    CHECK_FALSE(runtime.enabled("enter_house"));
    CHECK(runtime.set("enter_house", true));
    CHECK(runtime.enabled("enter_house"));
    CHECK_FALSE(runtime.enabled("open_gate"));
    CHECK_FALSE(runtime.set("missing", true));
    CHECK(runtime.set("enter_house", false));
    CHECK(runtime.enabled("enter_house"));
    CHECK_FALSE(runtime.enabled("open_gate"));
    runtime.clear();
    CHECK(runtime.size() == 0);
}

TEST_CASE("enable-trigger native dispatches and returns zero") {
    gl::assets::LvlDescription description;
    description.name = "enter_house";
    gl::assets::LvlDescription other;
    other.name = "closed_gate";
    const std::vector<gl::assets::LvlDescription> descriptions{description, other};

    TriggerRuntime runtime;
    runtime.reset(descriptions);
    NativeServices services;
    services.trigger = runtime.access();

    CHECK(call(0x7000009, {std::string{"enter_house"}, std::int32_t{1}}, services) ==
          Value{double{0.0}});
    CHECK(runtime.enabled("enter_house"));
    CHECK(call(0x7000009, {std::string{"enter_house"}, std::int32_t{0}}, services) ==
          Value{double{0.0}});
    CHECK(runtime.enabled("enter_house"));
    CHECK(call(0x7000009, {std::string{"enter_house"}, std::string{"x"}}, services) ==
          Value{double{0.0}});
    CHECK(call(0x7000009, {std::string{"closed_gate"}, double{1.0}}, services) ==
          Value{double{0.0}});
    CHECK(runtime.enabled("closed_gate"));
    CHECK(call(0x7000009, {std::string{"missing"}, std::int32_t{1}}, services) ==
          Value{double{0.0}});
    CHECK(call(0x7000009, {std::int32_t{1}, std::int32_t{1}}, services) == Value{double{0.0}});
}

TEST_CASE("enable-trigger native is registered with the server arity") {
    const auto *info = NativeRegistry::find(0x7000009);
    REQUIRE(info != nullptr);
    CHECK(info->support == gl::script::NativeSupport::supported);
    CHECK(info->arity == 2);
}

TEST_CASE("enable-trigger native fails without a runtime") {
    NativeServices services;
    CHECK(call(0x7000009, {std::string{"enter_house"}, std::int32_t{1}}, services) ==
          Value{double{0.0}});
}

TEST_CASE("real data: l1_1 trigger names load and toggle" *
          doctest::skip(std::getenv("GL_GAME_DIR") == nullptr)) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto level = gl::game::load_level(vfs.value(), "l1_1");
    REQUIRE(level.has_value());
    REQUIRE_FALSE(level->triggers.empty());

    TriggerRuntime runtime;
    runtime.reset(level->triggers);
    CHECK(runtime.size() == level->triggers.size());
    const std::string &name = level->triggers.front().name;
    REQUIRE_FALSE(name.empty());
    CHECK_FALSE(runtime.enabled(name));
    CHECK(runtime.set(name, true));
    CHECK(runtime.enabled(name));
}
