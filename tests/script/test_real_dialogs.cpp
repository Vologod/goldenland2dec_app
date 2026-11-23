#include "gl/assets/cs.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/game/game_script_host.hpp"
#include "gl/game/person_runtime.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/host.hpp"
#include "gl/script/script_vm.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <utility>
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
    CHECK(vm.answers_count() == 2);

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

TEST_CASE("real data: SetInjured call sites carry the Hero flag" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto script_data =
        vfs->read("scripts/dialogs/l1_2.p397_healer_experimentator.d397.age.cs");
    REQUIRE(script_data.has_value());
    auto nodes = parse_cs(script_data.value());
    REQUIRE(nodes.has_value());

    const auto &list = nodes.value();
    std::size_t calls = 0;
    for (const auto &node : list) {
        if (node.opcode != 48 || node.value != static_cast<double>(0x7000013)) continue;
        ++calls;
        REQUIRE(node.args[0] >= 0);
        REQUIRE(node.args[1] >= 0);
        const std::int32_t name_index = node.args[0];
        const std::int32_t flag_index = node.args[1];
        REQUIRE(static_cast<std::size_t>(name_index) < list.size());
        REQUIRE(static_cast<std::size_t>(flag_index) < list.size());
        CHECK(list[static_cast<std::size_t>(name_index)].text == "Hero");
        const double flag = list[static_cast<std::size_t>(flag_index)].value;
        CHECK((flag == 0.0 || flag == 1.0));
    }
    CHECK(calls == 4);
}

TEST_CASE("real data: SetInjured toggles the Hero effect list" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto script_data =
        vfs->read("scripts/dialogs/l1_2.p397_healer_experimentator.d397.age.cs");
    REQUIRE(script_data.has_value());
    auto parsed = parse_cs(script_data.value());
    REQUIRE(parsed.has_value());
    const auto &list = parsed.value();

    const auto run_call = [&list](double flag) {
        const auto found = [&]() -> std::size_t {
            for (std::size_t index = 0; index < list.size(); ++index) {
                const auto &node = list[index];
                if (node.opcode != 48 || node.value != static_cast<double>(0x7000013)) continue;
                const std::int32_t flag_index = node.args[1];
                if (flag_index < 0 ||
                    static_cast<std::size_t>(flag_index) >= list.size())
                    continue;
                if (list[static_cast<std::size_t>(flag_index)].value == flag)
                    return index;
            }
            return list.size();
        }();
        REQUIRE(found < list.size());

        std::vector<gl::assets::CsNode> nodes = list;
        nodes[0] = gl::assets::CsNode{};
        nodes[0].opcode = 49;
        nodes[0].c = static_cast<std::int32_t>(found);
        nodes[0].d = static_cast<std::int32_t>(found);

        auto runtime = gl::game::PersonRuntime::create({}, std::nullopt);
        gl::script::NativeServices services;
        services.person = runtime.access();
        gl::game::GameScriptHost host(services);
        gl::script::ScriptVm vm(std::move(nodes), host);
        vm.restart(true);
        CHECK(vm.step());
        CHECK(vm.step());
        CHECK_FALSE(vm.failed());
        return runtime.injured("Hero");
    };

    CHECK(run_call(1.0));
    CHECK_FALSE(run_call(0.0));
}

TEST_CASE("real data: GetDialogEnabled call sites use the ignored int argument" *
          doctest::skip(!game_dir_available())) {
    const char *dir = std::getenv("GL_GAME_DIR");
    REQUIRE(dir != nullptr);
    auto vfs = gl::assets::GameVfs::mount(dir);
    REQUIRE(vfs.has_value());
    auto script_data =
        vfs->read("scripts/dialogs/l12_1/l32.p214_feather_girl.d214.age.cs");
    REQUIRE(script_data.has_value());
    auto nodes = parse_cs(script_data.value());
    REQUIRE(nodes.has_value());

    const auto &list = nodes.value();
    std::size_t calls = 0;
    for (const auto &node : list) {
        if (node.opcode != 48 || node.value != static_cast<double>(0x700000e)) continue;
        ++calls;
        REQUIRE(node.args[0] >= 0);
        const std::size_t arg_index = static_cast<std::size_t>(node.args[0]);
        REQUIRE(arg_index < list.size());
        CHECK(list[arg_index].value == doctest::Approx(2.0));
    }
    CHECK(calls >= 1);
}
