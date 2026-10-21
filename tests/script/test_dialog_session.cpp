#include "gl/assets/cs.hpp"
#include "gl/assets/sdb.hpp"
#include "gl/script/dialog_session.hpp"
#include "gl/script/host.hpp"
#include "gl/script/script_vm.hpp"
#include <cstdint>
#include <doctest/doctest.h>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using gl::assets::CsNode;
using gl::assets::Sdb;
using gl::script::DefaultScriptHost;
using gl::script::DialogSession;
using gl::script::ScriptGlobal;
using gl::script::ScriptVm;
using gl::script::Value;
using gl::script::VmStatus;

namespace {
constexpr std::uint32_t kD_Say = 0x1000004;
constexpr std::uint32_t kD_CloseDialog = 0x1000005;
constexpr std::uint32_t kD_Answer = 0x1000006;

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

CsNode make_var(std::string name) {
    CsNode node = make_node(23);
    node.text = std::move(name);
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

CsNode make_compare_eq(std::int32_t left, std::int32_t right, std::int32_t on_true,
                       std::int32_t on_false) {
    CsNode node = make_node(7);
    node.a = left;
    node.b = right;
    node.c = on_true;
    node.d = on_false;
    return node;
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

Sdb make_sdb(std::initializer_list<std::pair<std::uint32_t, std::string_view>> entries) {
    std::vector<std::uint8_t> bytes{'S', 'D', 'B', ' '};
    for (const auto& [id, text] : entries) {
        append_u32(bytes, id);
        append_u32(bytes, static_cast<std::uint32_t>(text.size()));
        for (char ch : text) bytes.push_back(static_cast<std::uint8_t>(ch));
    }
    auto parsed = Sdb::parse(bytes);
    REQUIRE(parsed.has_value());
    return std::move(parsed.value());
}

std::vector<CsNode> two_page_dialog() {
    std::vector<CsNode> nodes(15);
    nodes[0] = make_compare_eq(1, 2, 3, 8);
    nodes[1] = make_var("LastAnswer");
    nodes[2] = make_num(2.0);
    nodes[3] = make_func(kD_Say, {4});
    nodes[4] = make_num(200.0);
    nodes[5] = make_jmp(-1, -1);
    nodes[8] = make_func(kD_Say, {9});
    nodes[9] = make_num(100.0);
    nodes[10] = make_func(kD_Answer, {11});
    nodes[11] = make_num(1.0);
    nodes[12] = make_func(kD_Answer, {13});
    nodes[13] = make_num(2.0);
    nodes[14] = make_jmp(-1, -1);
    return nodes;
}

struct SessionHarness {
    DefaultScriptHost host;
    ScriptVm vm;
    Sdb sdb;

    SessionHarness(std::vector<CsNode> nodes, Sdb database)
        : vm(std::move(nodes), host), sdb(std::move(database)) {
        vm.set_globals(std::vector<ScriptGlobal>{
            ScriptGlobal{"int", "LastAnswer", Value{std::int32_t{0}}}});
    }
};
} // namespace

TEST_CASE("dialog session: start maps the phrase and answers through the SDB") {
    SessionHarness h(two_page_dialog(),
                     make_sdb({{100, "Hello"}, {1, "Yes"}, {2, "No"}, {200, "Goodbye"}}));
    DialogSession session(h.vm, h.sdb);

    auto started = session.start();
    REQUIRE(started.has_value());
    CHECK(session.status() == VmStatus::wait_user);
    CHECK(session.waiting());
    CHECK(session.npc_text() == "Hello");

    const std::span<const gl::script::DialogLine> answers = session.answers();
    REQUIRE(answers.size() == 2);
    CHECK(answers[0].phrase_id == 1);
    CHECK(answers[0].text == "Yes");
    CHECK(answers[1].phrase_id == 2);
    CHECK(answers[1].text == "No");
}

TEST_CASE("dialog session: choose advances to the next page") {
    SessionHarness h(two_page_dialog(),
                     make_sdb({{100, "Hello"}, {1, "Yes"}, {2, "No"}, {200, "Goodbye"}}));
    DialogSession session(h.vm, h.sdb);
    REQUIRE(session.start().has_value());
    REQUIRE(session.waiting());

    auto chosen = session.choose(1);
    REQUIRE(chosen.has_value());
    CHECK(session.status() == VmStatus::wait_user);
    CHECK(session.waiting());
    CHECK(session.npc_text() == "Goodbye");
    CHECK(session.answers().empty());
}

TEST_CASE("dialog session: choose rejects an out-of-range answer") {
    SessionHarness h(two_page_dialog(),
                     make_sdb({{100, "Hello"}, {1, "Yes"}, {2, "No"}, {200, "Goodbye"}}));
    DialogSession session(h.vm, h.sdb);
    REQUIRE(session.start().has_value());
    REQUIRE(session.waiting());

    auto chosen = session.choose(99);
    REQUIRE_FALSE(chosen.has_value());
    CHECK(chosen.error().code == gl::ErrorCode::argument);
    CHECK(session.status() == VmStatus::wait_user);
    CHECK(session.waiting());
    CHECK(session.npc_text() == "Hello");
}

TEST_CASE("dialog session: missing SDB phrases fall back to placeholders") {
    std::vector<CsNode> nodes(5);
    nodes[0] = make_func(kD_Say, {1});
    nodes[1] = make_num(999.0);
    nodes[2] = make_func(kD_Answer, {3});
    nodes[3] = make_num(998.0);
    nodes[4] = make_jmp(-1, -1);

    SessionHarness h(std::move(nodes), make_sdb({{100, "Hello"}}));
    DialogSession session(h.vm, h.sdb);

    REQUIRE(session.start().has_value());
    REQUIRE(session.waiting());
    CHECK(session.npc_text() == "<phrase 999>");
    const std::span<const gl::script::DialogLine> answers = session.answers();
    REQUIRE(answers.size() == 1);
    CHECK(answers[0].phrase_id == 998);
    CHECK(answers[0].text == "<phrase 998>");
}

TEST_CASE("dialog session: a dialog that ends immediately leaves no page") {
    std::vector<CsNode> nodes(5);
    nodes[0] = make_func(kD_Say, {1});
    nodes[1] = make_num(100.0);
    nodes[2] = make_func(kD_CloseDialog, {3});
    nodes[3] = make_num(0.0);
    nodes[4] = make_jmp(-1, -1);

    SessionHarness h(std::move(nodes), make_sdb({{100, "Hello"}}));
    DialogSession session(h.vm, h.sdb);

    auto started = session.start();
    REQUIRE(started.has_value());
    CHECK(session.status() == VmStatus::end);
    CHECK_FALSE(session.waiting());
    CHECK(session.npc_text().empty());
    CHECK(session.answers().empty());
}

TEST_CASE("dialog session: a failure while starting is an error") {
    std::vector<CsNode> nodes(1);
    nodes[0] = make_node(3);

    SessionHarness h(std::move(nodes), make_sdb({}));
    DialogSession session(h.vm, h.sdb);

    auto started = session.start();
    REQUIRE_FALSE(started.has_value());
    CHECK(started.error().code == gl::ErrorCode::format);
    CHECK(h.vm.failed());
    CHECK(session.status() == VmStatus::end);
    CHECK_FALSE(session.waiting());
}

TEST_CASE("dialog session: a failure after choosing is an error") {
    std::vector<CsNode> nodes(11);
    nodes[0] = make_compare_eq(1, 2, 3, 6);
    nodes[1] = make_var("LastAnswer");
    nodes[2] = make_num(1.0);
    nodes[3] = make_node(3);
    nodes[6] = make_func(kD_Say, {7});
    nodes[7] = make_num(100.0);
    nodes[8] = make_func(kD_Answer, {9});
    nodes[9] = make_num(1.0);
    nodes[10] = make_jmp(-1, -1);

    SessionHarness h(std::move(nodes), make_sdb({{100, "Hello"}, {1, "Yes"}}));
    DialogSession session(h.vm, h.sdb);
    REQUIRE(session.start().has_value());
    REQUIRE(session.waiting());

    auto chosen = session.choose(0);
    REQUIRE_FALSE(chosen.has_value());
    CHECK(chosen.error().code == gl::ErrorCode::format);
    CHECK(h.vm.failed());
    CHECK(session.status() == VmStatus::end);
    CHECK_FALSE(session.waiting());
}

TEST_CASE("dialog session: start resets runtime dialog state") {
    SessionHarness h(two_page_dialog(),
                     make_sdb({{100, "Hello"}, {1, "Yes"}, {2, "No"}, {200, "Goodbye"}}));
    DialogSession session(h.vm, h.sdb);
    REQUIRE(session.start().has_value());
    REQUIRE(session.waiting());
    REQUIRE(session.choose(1).has_value());
    REQUIRE(session.npc_text() == "Goodbye");

    REQUIRE(session.start().has_value());
    CHECK(session.npc_text() == "Hello");
    const Value* last_answer = h.vm.variable("LastAnswer");
    REQUIRE(last_answer != nullptr);
    REQUIRE(std::holds_alternative<std::int32_t>(*last_answer));
    CHECK(std::get<std::int32_t>(*last_answer) == 0);
}

TEST_CASE("dialog session: choose without a waiting dialog is rejected") {
    std::vector<CsNode> nodes(3);
    nodes[0] = make_func(kD_Say, {1});
    nodes[1] = make_num(100.0);
    nodes[2] = make_jmp(-1, -1);

    SessionHarness h(std::move(nodes), make_sdb({{100, "Hello"}}));
    DialogSession session(h.vm, h.sdb);

    auto chosen = session.choose(0);
    REQUIRE_FALSE(chosen.has_value());
    CHECK(chosen.error().code == gl::ErrorCode::argument);
    CHECK_FALSE(session.waiting());
    CHECK(session.npc_text().empty());
    CHECK(session.answers().empty());
}
