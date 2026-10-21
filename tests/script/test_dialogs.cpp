#include "gl/assets/cs.hpp"
#include "gl/script/host.hpp"
#include "gl/script/script_vm.hpp"
#include <doctest/doctest.h>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <utility>
#include <variant>
#include <vector>

using gl::assets::CsNode;
using gl::script::DefaultScriptHost;
using gl::script::ScriptGlobal;
using gl::script::ScriptVm;
using gl::script::Value;
using gl::script::VmStatus;

namespace {
constexpr std::uint32_t kExit = 0x1000000;
constexpr std::uint32_t kSignal = 0x1000001;
constexpr std::uint32_t kConsole = 0x1000002;
constexpr std::uint32_t kCmd = 0x1000003;
constexpr std::uint32_t kD_Say = 0x1000004;
constexpr std::uint32_t kD_CloseDialog = 0x1000005;
constexpr std::uint32_t kD_Answer = 0x1000006;
constexpr std::uint32_t kD_PlaySound = 0x1000007;
constexpr std::uint32_t kPersonParameter = 0x4000000;
constexpr std::uint32_t kShowMessage = 0x400000B;

CsNode make_node(std::int32_t opcode) {
    CsNode node;
    node.opcode = opcode;
    return node;
}

CsNode make_num(double value) {
    CsNode node = make_node(24);
    node.value = value;
    return node;
}

CsNode make_func(std::uint32_t id, std::initializer_list<std::int32_t> args) {
    CsNode node = make_node(48);
    node.value = static_cast<double>(id);
    std::size_t i = 0;
    for (std::int32_t arg : args) {
        if (i < node.args.size()) node.args[i++] = arg;
    }
    return node;
}

CsNode make_jmp(std::int32_t target, std::int32_t next = -1) {
    CsNode node = make_node(49);
    node.c = target;
    node.d = next;
    return node;
}

std::vector<CsNode> synthetic_dialog() {
    std::vector<CsNode> nodes(7);
    nodes[0] = make_func(kD_Say, {1});
    nodes[1] = make_num(100.0);
    nodes[2] = make_func(kD_Answer, {3});
    nodes[3] = make_num(1.0);
    nodes[4] = make_func(kD_Answer, {5});
    nodes[5] = make_num(2.0);
    nodes[6] = make_jmp(-1, -1);
    return nodes;
}

struct DialogHarness {
    DefaultScriptHost host;
    ScriptVm vm;

    explicit DialogHarness(std::vector<CsNode> nodes, std::vector<ScriptGlobal> globals = {})
        : vm(std::move(nodes), host) {
        vm.set_globals(globals);
        vm.restart(true);
    }
};
} // namespace

TEST_CASE("dialogs: D_Say and D_Answer reach wait_user and expose ids") {
    DialogHarness h(synthetic_dialog());
    while (h.vm.step()) {
    }

    REQUIRE(h.vm.status() == VmStatus::wait_user);
    CHECK(h.vm.answers_count() == 2);
    CHECK(h.vm.dialogs_data() == std::vector<std::uint32_t>{100, 1, 2});
}

TEST_CASE("dialogs: user_input sets LastPhrase/LastAnswer and restarts") {
    DialogHarness h(synthetic_dialog());
    while (h.vm.step()) {
    }
    REQUIRE(h.vm.status() == VmStatus::wait_user);

    h.vm.user_input(0);
    CHECK(h.vm.status() == VmStatus::restart);
    CHECK(h.vm.dialogs_data().empty());

    Value* phrase = h.vm.variable("LastPhrase");
    REQUIRE(phrase != nullptr);
    REQUIRE(std::holds_alternative<std::uint32_t>(*phrase));
    CHECK(std::get<std::uint32_t>(*phrase) == 100);

    Value* answer = h.vm.variable("LastAnswer");
    REQUIRE(answer != nullptr);
    REQUIRE(std::holds_alternative<std::uint32_t>(*answer));
    CHECK(std::get<std::uint32_t>(*answer) == 1);

    CHECK(h.vm.step());
    CHECK(h.vm.status() == VmStatus::continue_);

    while (h.vm.step()) {
    }
    CHECK(h.vm.status() == VmStatus::wait_user);
    CHECK(h.vm.dialogs_data() == std::vector<std::uint32_t>{100, 1, 2});
}

TEST_CASE("dialogs: user_input(1) selects the second answer") {
    DialogHarness h(synthetic_dialog());
    while (h.vm.step()) {
    }
    REQUIRE(h.vm.status() == VmStatus::wait_user);

    h.vm.user_input(1);
    CHECK(h.vm.status() == VmStatus::restart);

    const Value* phrase = h.vm.variable("LastPhrase");
    REQUIRE(phrase != nullptr);
    REQUIRE(std::holds_alternative<std::uint32_t>(*phrase));
    CHECK(std::get<std::uint32_t>(*phrase) == 100);

    const Value* answer = h.vm.variable("LastAnswer");
    REQUIRE(answer != nullptr);
    REQUIRE(std::holds_alternative<std::uint32_t>(*answer));
    CHECK(std::get<std::uint32_t>(*answer) == 2);
}

TEST_CASE("dialogs: user_input rejects an out-of-range answer") {
    DialogHarness h(synthetic_dialog());
    while (h.vm.step()) {
    }
    REQUIRE(h.vm.status() == VmStatus::wait_user);

    h.vm.user_input(99);
    CHECK(h.vm.status() == VmStatus::wait_user);
    CHECK(h.vm.dialogs_data() == std::vector<std::uint32_t>{100, 1, 2});
    CHECK(h.vm.variable("LastPhrase") == nullptr);
    CHECK(h.vm.variable("LastAnswer") == nullptr);
}

TEST_CASE("dialogs: user_input without recorded answers is a no-op") {
    std::vector<CsNode> nodes(3);
    nodes[0] = make_func(kD_Say, {1});
    nodes[1] = make_num(100.0);
    nodes[2] = make_jmp(-1, -1);

    DialogHarness h(std::move(nodes));
    while (h.vm.step()) {
    }
    REQUIRE(h.vm.status() == VmStatus::wait_user);
    CHECK(h.vm.answers_count() == 0);

    h.vm.user_input(0);
    CHECK(h.vm.status() == VmStatus::wait_user);
    CHECK(h.vm.dialogs_data() == std::vector<std::uint32_t>{100});
    CHECK(h.vm.variable("LastPhrase") == nullptr);
}

TEST_CASE("dialogs: D_CloseDialog after D_Say ends execution") {
    std::vector<CsNode> nodes(5);
    nodes[0] = make_func(kD_Say, {1});
    nodes[1] = make_num(100.0);
    nodes[2] = make_func(kD_CloseDialog, {3});
    nodes[3] = make_num(0.0);
    nodes[4] = make_jmp(-1, -1);

    DialogHarness h(std::move(nodes));
    CHECK(h.vm.step());
    CHECK(h.vm.step());
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
    CHECK(h.vm.dialogs_data().empty());
}

TEST_CASE("host: known and unknown function stubs return zero") {
    DefaultScriptHost host;
    const std::span<const Value> no_args;

    const std::uint32_t known[] = {kPersonParameter,
                                   kExit,
                                   kSignal,
                                   kConsole,
                                   kCmd,
                                   kD_PlaySound,
                                   kShowMessage};
    for (std::uint32_t id : known) {
        CHECK(gl::assets::cs_function_is_known(id));
        const Value value = host.call(id, no_args);
        REQUIRE(std::holds_alternative<std::int32_t>(value));
        CHECK(std::get<std::int32_t>(value) == 0);
    }

    CHECK_FALSE(gl::assets::cs_function_is_known(0xDEADBEEFu));
    const Value unknown = host.call(0xDEADBEEFu, no_args);
    REQUIRE(std::holds_alternative<std::int32_t>(unknown));
    CHECK(std::get<std::int32_t>(unknown) == 0);
}
