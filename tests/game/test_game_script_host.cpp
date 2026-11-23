#include <doctest/doctest.h>
#include "gl/game/game_script_host.hpp"
#include "gl/assets/cs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

TEST_CASE("game script host logs an unsupported native once") {
    std::vector<std::string> messages;
    gl::script::NativeServices services;
    services.log = [&](std::string_view text) { messages.emplace_back(text); };
    gl::game::GameScriptHost host(services);
    const auto args = std::span<const gl::script::Value>{};
    CHECK(std::get<std::int32_t>(host.call(0x4000002, args)) == 0);
    CHECK(std::get<std::int32_t>(host.call(0x4000002, args)) == 0);
    REQUIRE(messages.size() == 1);
    CHECK(messages.front().find("RS_AddPerson_1") != std::string::npos);
    CHECK(messages.front().find("unimplemented") != std::string::npos);
}

TEST_CASE("game script host dispatches supported natives") {
    std::string console;
    gl::script::NativeServices services;
    services.console = [&](std::string_view text) { console = std::string(text); };
    gl::game::GameScriptHost host(services);
    const gl::script::Value argument = std::string("hello");
    host.call(0x1000002, {&argument, 1});
    CHECK(console == "hello");
}

TEST_CASE("game script host ignores unknown ids") {
    gl::game::GameScriptHost host;
    const auto args = std::span<const gl::script::Value>{};
    CHECK(std::get<std::int32_t>(host.call(0xDEADBEEF, args)) == 0);
}

TEST_CASE("game script host ignores vm-internal natives") {
    gl::game::GameScriptHost host;
    const auto args = std::span<const gl::script::Value>{};
    CHECK(std::get<std::int32_t>(host.call(0x1000004, args)) == 0);
}

TEST_CASE("game script host dispatches Signal") {
    std::string signal_text;
    gl::script::NativeServices services;
    services.signal = [&](std::string_view text) { signal_text = std::string(text); };
    gl::game::GameScriptHost host(services);
    const gl::script::Value argument = std::string("alarm");
    host.call(0x1000001, {&argument, 1});
    CHECK(signal_text == "alarm");
}

TEST_CASE("game script host requests stop on a nonzero Exit") {
    gl::game::GameScriptHost host;
    const gl::script::Value code = std::int32_t{1};
    host.call(0x1000000, {&code, 1});
    CHECK(host.stop_requested());
}

TEST_CASE("game script host keeps running on a zero Exit") {
    gl::game::GameScriptHost host;
    const gl::script::Value code = std::int32_t{0};
    host.call(0x1000000, {&code, 1});
    CHECK_FALSE(host.stop_requested());
}

namespace {
gl::assets::CsNode dialog_node(std::int32_t opcode) {
    gl::assets::CsNode node;
    node.opcode = opcode;
    return node;
}
gl::assets::CsNode dialog_num(double value) {
    gl::assets::CsNode node = dialog_node(24);
    node.value = value;
    return node;
}
gl::assets::CsNode dialog_str(std::string text) {
    gl::assets::CsNode node = dialog_node(22);
    node.text = std::move(text);
    return node;
}
gl::assets::CsNode dialog_str_var(std::string text) {
    gl::assets::CsNode node = dialog_node(23);
    node.text = std::move(text);
    return node;
}
gl::assets::CsNode dialog_func(std::uint32_t id,
                               std::initializer_list<std::int32_t> args) {
    gl::assets::CsNode node = dialog_node(48);
    node.value = static_cast<double>(id);
    std::size_t index = 0;
    for (std::int32_t arg : args) node.args[index++] = arg;
    return node;
}
gl::assets::CsNode dialog_assign(std::int32_t dest, std::int32_t source,
                                 std::int32_t next) {
    gl::assets::CsNode node = dialog_node(50);
    node.a = dest;
    node.b = source;
    node.d = next;
    return node;
}
gl::assets::CsNode dialog_jmp(std::int32_t target, std::int32_t next) {
    gl::assets::CsNode node = dialog_node(49);
    node.c = target;
    node.d = next;
    return node;
}
} // namespace

TEST_CASE("game script host runs GetDialogEnabled through the VM") {
    std::vector<gl::assets::CsNode> nodes(5);
    nodes[0] = dialog_assign(1, 2, 3);
    nodes[1] = dialog_str_var("result");
    nodes[2] = dialog_func(0x700000e, {4});
    nodes[3] = dialog_jmp(-1, -1);
    nodes[4] = dialog_num(2.0);

    gl::script::NativeServices services;
    bool gate = true;
    double probability = -1.0;
    services.person.dialog_percent = [] { return std::int32_t{37}; };
    services.random.coin = [&](double value) {
        probability = value;
        return gate;
    };

    gl::game::GameScriptHost host(services);
    gl::script::ScriptVm vm(std::move(nodes), host);
    const std::vector<gl::script::ScriptGlobal> globals{
        {"float", "result", double{0.0}}};
    vm.set_globals(globals);
    vm.restart(true);

    CHECK(vm.step());
    const gl::script::Value *result = vm.variable("result");
    REQUIRE(result != nullptr);
    REQUIRE(std::holds_alternative<double>(*result));
    CHECK(std::get<double>(*result) == doctest::Approx(1.0));
    CHECK(probability == doctest::Approx(0.37));

    gate = false;
    vm.restart(true);
    CHECK(vm.step());
    const gl::script::Value *again = vm.variable("result");
    REQUIRE(again != nullptr);
    REQUIRE(std::holds_alternative<double>(*again));
    CHECK(std::get<double>(*again) == doctest::Approx(0.0));
}

TEST_CASE("game script host runs SetInjured through the VM") {
    std::vector<gl::assets::CsNode> nodes(4);
    nodes[0] = dialog_func(0x7000013, {1, 2});
    nodes[1] = dialog_str("Hero");
    nodes[2] = dialog_num(1.0);
    nodes[3] = dialog_jmp(-1, -1);

    gl::script::NativeServices services;
    std::string name;
    bool flag = false;
    services.person.set_injured = [&](std::string_view person, bool injured) {
        name = std::string(person);
        flag = injured;
    };

    gl::game::GameScriptHost host(services);
    gl::script::ScriptVm vm(std::move(nodes), host);
    vm.restart(true);
    while (vm.step()) {
    }
    CHECK(name == "Hero");
    CHECK(flag);
}
