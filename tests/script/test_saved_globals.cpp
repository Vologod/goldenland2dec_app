#include "gl/assets/cs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
struct NullHost : gl::script::ScriptHost {
    gl::script::Value call(std::uint32_t, std::span<const gl::script::Value>) override {
        return std::int32_t{0};
    }
};

gl::script::ScriptGlobal global(std::string name, gl::script::Value value) {
    gl::script::ScriptGlobal out;
    out.name = std::move(name);
    out.value = std::move(value);
    return out;
}

gl::assets::CsNode variable_node(std::string name) {
    gl::assets::CsNode node;
    node.opcode = 23;
    node.text = std::move(name);
    return node;
}
} // namespace

TEST_CASE("saved globals coerce to the declared variant type") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    const std::vector<gl::script::ScriptGlobal> initial{
        global("i", std::int32_t{1}),
        global("d", std::uint32_t{2}),
        global("f", 0.5),
        global("s", std::string{"a"}),
    };
    vm.set_globals(initial);

    const std::vector<gl::script::ScriptGlobal> overlay{
        global("i", 7.9),
        global("d", -1.0),
        global("f", 2.25),
        global("s", std::string{"b"}),
    };
    const auto counts = vm.apply_saved_globals(overlay);
    CHECK(counts.matched == 4);
    CHECK(counts.added == 0);
    CHECK(counts.kept == 0);
    CHECK(std::get<std::int32_t>(vm.globals().at("i")) == 7);
    CHECK(std::get<std::uint32_t>(vm.globals().at("d")) == 0u);
    CHECK(std::get<double>(vm.globals().at("f")) == 2.25);
    CHECK(std::get<std::string>(vm.globals().at("s")) == "b");
}

TEST_CASE("saved globals add unknown names and keep type mismatches") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    const std::vector<gl::script::ScriptGlobal> initial{
        global("i", std::int32_t{1}),
        global("s", std::string{"a"}),
    };
    vm.set_globals(initial);

    const std::vector<gl::script::ScriptGlobal> overlay{
        global("new_number", 3.5),
        global("new_text", std::string{"x"}),
        global("i", std::string{"bad"}),
        global("s", 4.0),
    };
    const auto counts = vm.apply_saved_globals(overlay);
    CHECK(counts.matched == 0);
    CHECK(counts.added == 2);
    CHECK(counts.kept == 2);
    CHECK(std::get<double>(vm.globals().at("new_number")) == 3.5);
    CHECK(std::get<std::string>(vm.globals().at("new_text")) == "x");
    CHECK(std::get<std::int32_t>(vm.globals().at("i")) == 1);
    CHECK(std::get<std::string>(vm.globals().at("s")) == "a");
}

TEST_CASE("saved globals use the last duplicate value") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    const std::vector<gl::script::ScriptGlobal> initial{global("i", std::int32_t{1})};
    vm.set_globals(initial);
    const std::vector<gl::script::ScriptGlobal> overlay{
        global("i", 2.0),
        global("i", 3.0),
    };
    vm.apply_saved_globals(overlay);
    CHECK(std::get<std::int32_t>(vm.globals().at("i")) == 3);
}

TEST_CASE("saved globals rebuild node variables") {
    NullHost host;
    std::vector<gl::assets::CsNode> nodes{variable_node("g")};
    gl::script::ScriptVm vm(std::move(nodes), host);
    const std::vector<gl::script::ScriptGlobal> initial{global("g", std::int32_t{1})};
    vm.set_globals(initial);
    const std::vector<gl::script::ScriptGlobal> overlay{global("g", 9.0)};
    vm.apply_saved_globals(overlay);
    REQUIRE(vm.variable("g") != nullptr);
    CHECK(std::get<std::int32_t>(*vm.variable("g")) == 9);
}
