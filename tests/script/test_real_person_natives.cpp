#include <doctest/doctest.h>
#include "gl/assets/cs.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/game_script_host.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"
#include <cstdint>
#include <cstdlib>
#include <string>
#include <variant>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("real dialog reads and writes Hero reputation through person natives" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto script = vfs->read("scripts/dialogs/old_test.age.cs");
    auto globals_data = vfs->read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(script.has_value());
    REQUIRE(globals_data.has_value());
    auto nodes = gl::assets::parse_cs(script.value());
    auto globals = gl::script::parse_globals(globals_data.value());
    REQUIRE(nodes.has_value());
    REQUIRE(globals.has_value());

    gl::game::PersonRuntime runtime = gl::game::PersonRuntime::create({}, std::nullopt);
    gl::script::NativeServices services;
    services.person = runtime.access();
    gl::game::GameScriptHost host(services);

    runtime.set_parameter("Hero", gl::script::PersonParameter::reputation, 40.0);
    gl::script::ScriptVm vm(std::move(nodes.value()), host);
    vm.set_globals(globals.value());

    while (vm.step()) {
    }
    REQUIRE(vm.status() == gl::script::VmStatus::wait_user);
    REQUIRE(vm.answers_count() == 3);
    REQUIRE(vm.user_input(2));
    for (int i = 0; i < 2000 && vm.status() != gl::script::VmStatus::wait_user; ++i) {
        if (!vm.step()) break;
    }
    REQUIRE(vm.status() == gl::script::VmStatus::wait_user);

    const gl::script::Value *slava = vm.variable("slava");
    REQUIRE(slava != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*slava));
    CHECK(std::get<std::int32_t>(*slava) == 38);
    CHECK(runtime.get_parameter("Hero", gl::script::PersonParameter::reputation) ==
          doctest::Approx(38.0));
}
