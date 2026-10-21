#include "gl/assets/cs.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/host.hpp"
#include "gl/script/script_vm.hpp"
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <variant>
#include <vector>

using gl::assets::parse_cs;
using gl::script::DefaultScriptHost;
using gl::script::parse_globals;
using gl::script::ScriptVm;
using gl::script::VmStatus;

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }
} // namespace

TEST_CASE("real data: demon dialog reaches wait_user and accepts an answer" *
          doctest::skip(!game_dir_available())) {
    const char* dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);

    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());

    auto script_data = vfs->read("scripts/dialogs/demon.d1.age.cs");
    REQUIRE(script_data.has_value());
    auto nodes = parse_cs(script_data.value());
    REQUIRE(nodes.has_value());

    auto globals_data = vfs->read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(globals_data.has_value());
    auto globals = parse_globals(globals_data.value());
    REQUIRE(globals.has_value());

    DefaultScriptHost host;
    ScriptVm vm(std::move(nodes.value()), host);
    vm.set_globals(globals.value());
    vm.restart(true);

    while (vm.step()) {
    }
    REQUIRE(vm.status() == VmStatus::wait_user);

    const std::vector<std::uint32_t> data = vm.dialogs_data();
    REQUIRE(data.size() >= 2);
    for (std::uint32_t id : data) CHECK(id > 0);
    CHECK(vm.answers_count() == data.size() - 1);

    vm.user_input(0);
    CHECK(vm.status() == VmStatus::restart);

    const auto* phrase = vm.variable("LastPhrase");
    REQUIRE(phrase != nullptr);
    REQUIRE(std::holds_alternative<std::uint32_t>(*phrase));
    CHECK(std::get<std::uint32_t>(*phrase) == data[0]);

    const auto* answer = vm.variable("LastAnswer");
    REQUIRE(answer != nullptr);
    REQUIRE(std::holds_alternative<std::uint32_t>(*answer));
    CHECK(std::get<std::uint32_t>(*answer) == data[1]);

    while (vm.step()) {
    }
    CHECK((vm.status() == VmStatus::end || vm.status() == VmStatus::wait_user));
}
