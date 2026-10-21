#include "gl/script/script_vm.hpp"
#include <doctest/doctest.h>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

using gl::assets::CsNode;
using gl::script::ScriptGlobal;
using gl::script::ScriptHost;
using gl::script::ScriptVm;
using gl::script::Value;
using gl::script::VmStatus;

namespace {
constexpr std::uint32_t kD_Say = 0x1000004;
constexpr std::uint32_t kD_CloseDialog = 0x1000005;
constexpr std::uint32_t kPersonParameter = 0x4000001;

struct RecordingHost final : ScriptHost {
    struct Call {
        std::uint32_t id;
        std::vector<Value> args;
    };
    std::vector<Call> calls;
    Value result = std::int32_t{0};

    Value call(std::uint32_t id, std::span<const Value> args) override {
        calls.push_back(Call{id, std::vector<Value>(args.begin(), args.end())});
        return result;
    }
};

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

CsNode make_num_var(double value) {
    CsNode node = make_node(21);
    node.value = value;
    return node;
}

CsNode make_str(std::string text) {
    CsNode node = make_node(22);
    node.text = std::move(text);
    return node;
}

CsNode make_str_var(std::string text) {
    CsNode node = make_node(23);
    node.text = std::move(text);
    return node;
}

CsNode make_bin(std::int32_t opcode, std::int32_t a, std::int32_t b, std::int32_t c = -1,
                std::int32_t d = -1) {
    CsNode node = make_node(opcode);
    node.a = a;
    node.b = b;
    node.c = c;
    node.d = d;
    return node;
}

CsNode make_assign(std::int32_t dest, std::int32_t source, std::int32_t next) {
    CsNode node = make_node(50);
    node.a = dest;
    node.b = source;
    node.d = next;
    return node;
}

CsNode make_jmp(std::int32_t target, std::int32_t next) {
    CsNode node = make_node(49);
    node.c = target;
    node.d = next;
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

ScriptGlobal global_int(std::string name, std::int32_t value) {
    return ScriptGlobal{"int", std::move(name), value};
}

ScriptGlobal global_string(std::string name, std::string value) {
    return ScriptGlobal{"string", std::move(name), std::move(value)};
}

struct VmHarness {
    RecordingHost host;
    ScriptVm vm;

    VmHarness(std::vector<CsNode> nodes, std::vector<ScriptGlobal> globals)
        : vm(std::move(nodes), host) {
        vm.set_globals(globals);
        vm.restart(true);
    }
};
} // namespace

TEST_CASE("vm: assign evaluates arithmetic with precedence") {
    std::vector<CsNode> nodes(8);
    nodes[0] = make_assign(1, 2, 7);
    nodes[1] = make_str_var("x");
    nodes[2] = make_bin(14, 3, 4);
    nodes[3] = make_num(1.0);
    nodes[4] = make_bin(16, 5, 6);
    nodes[5] = make_num(2.0);
    nodes[6] = make_num(3.0);
    nodes[7] = make_jmp(-1, -1);

    VmHarness h(std::move(nodes), {global_int("x", 0)});
    CHECK(h.vm.step());
    CHECK(h.vm.status() == VmStatus::continue_);

    Value* x = h.vm.variable("x");
    REQUIRE(x != nullptr);
    REQUIRE(std::holds_alternative<double>(*x));
    CHECK(std::get<double>(*x) == doctest::Approx(7.0));

    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
    CHECK(h.vm.steps() == 2);
}

TEST_CASE("vm: assign of a numeric literal stores an int") {
    std::vector<CsNode> nodes(4);
    nodes[0] = make_assign(1, 2, 3);
    nodes[1] = make_str_var("x");
    nodes[2] = make_num(5.0);
    nodes[3] = make_jmp(-1, -1);

    VmHarness h(std::move(nodes), {global_int("x", 0)});
    CHECK(h.vm.step());

    Value* x = h.vm.variable("x");
    REQUIRE(x != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*x));
    CHECK(std::get<std::int32_t>(*x) == 5);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: arithmetic casts a numeric literal to the left type") {
    std::vector<CsNode> nodes(6);
    nodes[0] = make_assign(1, 2, 3);
    nodes[1] = make_str_var("y");
    nodes[2] = make_bin(14, 4, 5);
    nodes[3] = make_jmp(-1, -1);
    nodes[4] = make_str_var("x");
    nodes[5] = make_num(2.9);

    VmHarness h(std::move(nodes), {global_int("x", 2), global_int("y", 0)});
    CHECK(h.vm.step());

    Value* y = h.vm.variable("y");
    REQUIRE(y != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*y));
    CHECK(std::get<std::int32_t>(*y) == 4);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: comparison casts the right operand to the left type and branches") {
    std::vector<CsNode> nodes(9);
    nodes[0] = make_bin(7, 1, 2, 3, 4);
    nodes[1] = make_str_var("x");
    nodes[2] = make_num(2.9);
    nodes[3] = make_assign(5, 6, 7);
    nodes[4] = make_assign(5, 8, 7);
    nodes[5] = make_str_var("y");
    nodes[6] = make_num(10.0);
    nodes[7] = make_jmp(-1, -1);
    nodes[8] = make_num(20.0);

    VmHarness h(std::move(nodes), {global_int("x", 2), global_int("y", 0)});
    CHECK(h.vm.step());
    CHECK(h.vm.step());

    Value* y = h.vm.variable("y");
    REQUIRE(y != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*y));
    CHECK(std::get<std::int32_t>(*y) == 10);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: logical and takes the true branch") {
    std::vector<CsNode> nodes(13);
    nodes[0] = make_bin(2, 1, 2, 3, 4);
    nodes[1] = make_bin(7, 5, 6);
    nodes[2] = make_bin(11, 7, 8);
    nodes[3] = make_assign(9, 10, 11);
    nodes[4] = make_assign(9, 12, 11);
    nodes[5] = make_str_var("x");
    nodes[6] = make_num(2.0);
    nodes[7] = make_str_var("y");
    nodes[8] = make_num(10.0);
    nodes[9] = make_str_var("z");
    nodes[10] = make_num(1.0);
    nodes[11] = make_jmp(-1, -1);
    nodes[12] = make_num(0.0);

    VmHarness h(std::move(nodes),
                {global_int("x", 2), global_int("y", 5), global_int("z", 0)});
    CHECK(h.vm.step());
    CHECK(h.vm.step());

    Value* z = h.vm.variable("z");
    REQUIRE(z != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*z));
    CHECK(std::get<std::int32_t>(*z) == 1);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: logical and takes the false branch") {
    std::vector<CsNode> nodes(13);
    nodes[0] = make_bin(2, 1, 2, 3, 4);
    nodes[1] = make_bin(7, 5, 6);
    nodes[2] = make_bin(11, 7, 8);
    nodes[3] = make_assign(9, 10, 11);
    nodes[4] = make_assign(9, 12, 11);
    nodes[5] = make_str_var("x");
    nodes[6] = make_num(2.0);
    nodes[7] = make_str_var("y");
    nodes[8] = make_num(10.0);
    nodes[9] = make_str_var("z");
    nodes[10] = make_num(1.0);
    nodes[11] = make_jmp(-1, -1);
    nodes[12] = make_num(0.0);

    VmHarness h(std::move(nodes),
                {global_int("x", 3), global_int("y", 5), global_int("z", 5)});
    CHECK(h.vm.step());
    CHECK(h.vm.step());

    Value* z = h.vm.variable("z");
    REQUIRE(z != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*z));
    CHECK(std::get<std::int32_t>(*z) == 0);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: jmp loops backward and forward") {
    std::vector<CsNode> nodes(10);
    nodes[0] = make_jmp(1, 1);
    nodes[1] = make_assign(2, 3, 4);
    nodes[2] = make_str_var("x");
    nodes[3] = make_bin(14, 2, 5);
    nodes[4] = make_bin(11, 6, 7, 8, 9);
    nodes[5] = make_num(1.0);
    nodes[6] = make_str_var("x");
    nodes[7] = make_num(3.0);
    nodes[8] = make_jmp(1, 1);
    nodes[9] = make_jmp(-1, -1);

    VmHarness h(std::move(nodes), {global_int("x", 1)});
    CHECK(h.vm.step());
    CHECK(h.vm.step());
    CHECK(std::get<std::int32_t>(*h.vm.variable("x")) == 2);
    CHECK(h.vm.step());
    CHECK(h.vm.step());
    CHECK(h.vm.step());
    CHECK(std::get<std::int32_t>(*h.vm.variable("x")) == 3);
    CHECK(h.vm.step());
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
    CHECK(h.vm.steps() == 7);
}

TEST_CASE("vm: func calls the host and assigns its result") {
    std::vector<CsNode> nodes(6);
    nodes[0] = make_assign(1, 2, 4);
    nodes[1] = make_str_var("result");
    nodes[2] = make_func(kPersonParameter, {3, 5});
    nodes[3] = make_str("Hero");
    nodes[4] = make_jmp(-1, -1);
    nodes[5] = make_num(7.0);

    VmHarness h(std::move(nodes), {global_int("result", 0)});
    h.host.result = std::int32_t{42};

    CHECK(h.vm.step());
    REQUIRE(h.host.calls.size() == 1);
    CHECK(h.host.calls[0].id == kPersonParameter);
    REQUIRE(h.host.calls[0].args.size() == 2);
    REQUIRE(std::holds_alternative<std::string>(h.host.calls[0].args[0]));
    CHECK(std::get<std::string>(h.host.calls[0].args[0]) == "Hero");
    REQUIRE(std::holds_alternative<double>(h.host.calls[0].args[1]));
    CHECK(std::get<double>(h.host.calls[0].args[1]) == doctest::Approx(7.0));

    Value* result = h.vm.variable("result");
    REQUIRE(result != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*result));
    CHECK(std::get<std::int32_t>(*result) == 42);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: D_Say is recorded and jmp d=-1 waits for the user") {
    std::vector<CsNode> nodes(5);
    nodes[0] = make_assign(1, 2, 3);
    nodes[1] = make_str_var("result");
    nodes[2] = make_func(kD_Say, {4});
    nodes[3] = make_jmp(-1, -1);
    nodes[4] = make_num(111.0);

    VmHarness h(std::move(nodes), {global_int("result", 0)});
    CHECK(h.vm.step());
    CHECK(h.host.calls.empty());

    Value* result = h.vm.variable("result");
    REQUIRE(result != nullptr);
    CHECK(std::get<std::int32_t>(*result) == 0);

    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::wait_user);
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::wait_user);
}

TEST_CASE("vm: D_CloseDialog ends execution") {
    std::vector<CsNode> nodes(5);
    nodes[0] = make_assign(1, 2, 3);
    nodes[1] = make_str_var("result");
    nodes[2] = make_func(kD_CloseDialog, {4});
    nodes[3] = make_jmp(-1, -1);
    nodes[4] = make_num(1.0);

    VmHarness h(std::move(nodes), {global_int("result", 0)});
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
    CHECK(h.host.calls.empty());
}

TEST_CASE("vm: comparison does not evaluate the right operand after a left failure") {
    std::vector<CsNode> nodes(3);
    nodes[0] = make_bin(7, 99, 1, 2, 2);
    nodes[1] = make_func(kPersonParameter, {});
    nodes[2] = make_jmp(-1, -1);

    VmHarness h(std::move(nodes), {});
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
    CHECK(h.host.calls.empty());
}

TEST_CASE("vm: logical does not evaluate the right operand after a left failure") {
    std::vector<CsNode> nodes(3);
    nodes[0] = make_bin(2, 99, 1, 2, 2);
    nodes[1] = make_func(kPersonParameter, {});
    nodes[2] = make_jmp(-1, -1);

    VmHarness h(std::move(nodes), {});
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
    CHECK(h.host.calls.empty());
}

TEST_CASE("vm: top-level func ignores d and advances to the next node") {
    std::vector<CsNode> nodes(7);
    nodes[0] = make_func(kPersonParameter, {});
    nodes[0].d = 2;
    nodes[1] = make_assign(3, 4, 6);
    nodes[2] = make_assign(3, 5, 6);
    nodes[3] = make_str_var("x");
    nodes[4] = make_num(5.0);
    nodes[5] = make_num(7.0);
    nodes[6] = make_jmp(-1, -1);

    VmHarness h(std::move(nodes), {global_int("x", 0)});
    CHECK(h.vm.step());
    CHECK(h.vm.step());

    Value* x = h.vm.variable("x");
    REQUIRE(x != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*x));
    CHECK(std::get<std::int32_t>(*x) == 5);
}

TEST_CASE("vm: division by zero yields zero") {
    std::vector<CsNode> nodes(6);
    nodes[0] = make_assign(1, 2, 3);
    nodes[1] = make_str_var("x");
    nodes[2] = make_bin(17, 4, 5);
    nodes[3] = make_jmp(-1, -1);
    nodes[4] = make_str_var("a");
    nodes[5] = make_str_var("b");

    VmHarness h(std::move(nodes),
                {global_int("a", 5), global_int("b", 0), global_int("x", 9)});
    CHECK(h.vm.step());

    Value* x = h.vm.variable("x");
    REQUIRE(x != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*x));
    CHECK(std::get<std::int32_t>(*x) == 0);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: string comparison supports equality") {
    std::vector<CsNode> nodes(9);
    nodes[0] = make_bin(7, 1, 2, 3, 4);
    nodes[1] = make_str_var("s");
    nodes[2] = make_str("abc");
    nodes[3] = make_assign(5, 6, 7);
    nodes[4] = make_assign(5, 8, 7);
    nodes[5] = make_str_var("x");
    nodes[6] = make_num(1.0);
    nodes[7] = make_jmp(-1, -1);
    nodes[8] = make_num(0.0);

    VmHarness h(std::move(nodes), {global_string("s", "abc"), global_int("x", 0)});
    CHECK(h.vm.step());
    CHECK(h.vm.step());

    Value* x = h.vm.variable("x");
    REQUIRE(x != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*x));
    CHECK(std::get<std::int32_t>(*x) == 1);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: arithmetic rejects strings") {
    std::vector<CsNode> nodes(6);
    nodes[0] = make_assign(1, 2, 3);
    nodes[1] = make_str_var("x");
    nodes[2] = make_bin(14, 4, 5);
    nodes[3] = make_jmp(-1, -1);
    nodes[4] = make_str_var("s");
    nodes[5] = make_num(1.0);

    VmHarness h(std::move(nodes), {global_string("s", "abc"), global_int("x", 9)});
    CHECK(h.vm.step());

    Value* x = h.vm.variable("x");
    REQUIRE(x != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*x));
    CHECK(std::get<std::int32_t>(*x) == 0);
    CHECK_FALSE(h.vm.step());
}

TEST_CASE("vm: restart resets globals and dialog phrase variables") {
    std::vector<CsNode> nodes(6);
    nodes[0] = make_assign(1, 2, 3);
    nodes[1] = make_str_var("x");
    nodes[2] = make_num(9.0);
    nodes[3] = make_jmp(-1, -1);
    nodes[4] = make_str_var("LastPhrase");
    nodes[5] = make_str_var("LastAnswer");

    VmHarness h(std::move(nodes), {global_int("x", 5), global_int("LastPhrase", 7),
                                   global_int("LastAnswer", 8)});

    REQUIRE(h.vm.variable("x") != nullptr);
    CHECK(std::get<std::int32_t>(*h.vm.variable("x")) == 5);
    CHECK(std::get<std::int32_t>(*h.vm.variable("LastPhrase")) == 7);

    *h.vm.variable("x") = std::int32_t{99};
    h.vm.restart(false);
    CHECK(h.vm.status() == VmStatus::start);
    CHECK(h.vm.steps() == 0);
    CHECK(std::get<std::int32_t>(*h.vm.variable("x")) == 99);
    REQUIRE(std::holds_alternative<std::uint32_t>(*h.vm.variable("LastPhrase")));
    CHECK(std::get<std::uint32_t>(*h.vm.variable("LastPhrase")) == 0);
    CHECK(std::get<std::uint32_t>(*h.vm.variable("LastAnswer")) == 0);

    h.vm.restart(true);
    CHECK(std::get<std::int32_t>(*h.vm.variable("x")) == 5);
    CHECK(std::get<std::int32_t>(*h.vm.variable("LastPhrase")) == 7);
    CHECK(std::get<std::int32_t>(*h.vm.variable("LastAnswer")) == 8);
}

TEST_CASE("vm: malformed node indices end safely") {
    std::vector<CsNode> nodes(2);
    nodes[0] = make_jmp(9, 9);
    nodes[1] = make_num(0.0);

    VmHarness h(std::move(nodes), {});
    CHECK(h.vm.step());
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
}

TEST_CASE("vm: unsupported top-level opcode ends safely") {
    std::vector<CsNode> nodes(1);
    nodes[0] = make_node(3);

    VmHarness h(std::move(nodes), {});
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
}

TEST_CASE("vm: top-level numeric value node is a no-op") {
    std::vector<CsNode> nodes(2);
    nodes[0] = make_num_var(0.0);
    nodes[1] = make_jmp(-1, -1);

    VmHarness h(std::move(nodes), {});
    CHECK(h.vm.step());
    CHECK_FALSE(h.vm.step());
    CHECK(h.vm.status() == VmStatus::end);
}

TEST_CASE("vm: step limit reports infinity") {
    std::vector<CsNode> nodes(2);
    nodes[0] = make_jmp(0, 0);
    nodes[1] = make_num(0.0);

    VmHarness h(std::move(nodes), {});
    std::uint64_t executed = 0;
    while (h.vm.step()) {
        ++executed;
        if (executed > 10'000'000) break;
    }

    CHECK(executed == 10'000'000);
    CHECK(h.vm.steps() == 10'000'000);
    CHECK(h.vm.status() == VmStatus::infinity);
}
