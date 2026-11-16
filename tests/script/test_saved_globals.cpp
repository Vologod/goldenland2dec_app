#include "gl/assets/cs.hpp"
#include "gl/assets/save_payload.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"

#include <cstdint>
#include <cstring>
#include <doctest/doctest.h>
#include <limits>
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

TEST_CASE("saved globals clamp numeric conversions") {
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    const std::vector<gl::script::ScriptGlobal> initial{
        global("imax", std::int32_t{0}),
        global("imin", std::int32_t{0}),
        global("inan", std::int32_t{0}),
        global("umax", std::uint32_t{0}),
        global("uneg", std::uint32_t{0}),
    };
    vm.set_globals(initial);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<gl::script::ScriptGlobal> overlay{
        global("imax", static_cast<double>(std::numeric_limits<std::int32_t>::max()) + 1.0),
        global("imin", static_cast<double>(std::numeric_limits<std::int32_t>::min()) - 1.0),
        global("inan", nan),
        global("umax", static_cast<double>(std::numeric_limits<std::uint32_t>::max()) + 1.0),
        global("uneg", -1.0),
    };
    vm.apply_saved_globals(overlay);
    CHECK(std::get<std::int32_t>(vm.globals().at("imax")) ==
          std::numeric_limits<std::int32_t>::max());
    CHECK(std::get<std::int32_t>(vm.globals().at("imin")) ==
          std::numeric_limits<std::int32_t>::min());
    CHECK(std::get<std::int32_t>(vm.globals().at("inan")) == 0);
    CHECK(std::get<std::uint32_t>(vm.globals().at("umax")) ==
          std::numeric_limits<std::uint32_t>::max());
    CHECK(std::get<std::uint32_t>(vm.globals().at("uneg")) == 0u);
}

TEST_CASE("save_globals_from_vars maps numeric, string and drops unknown types") {
    gl::assets::SaveVar number;
    number.type = 0;
    number.name = "n";
    const double value = 2.5;
    std::memcpy(number.value.data(), &value, sizeof(value));

    gl::assets::SaveVar text;
    text.type = 3;
    text.name = "s";
    text.text_value = "x";

    gl::assets::SaveVar unknown;
    unknown.type = 4;
    unknown.name = "b";

    const std::vector<gl::assets::SaveVar> vars{number, text, unknown};
    const auto globals = gl::script::save_globals_from_vars(vars);
    REQUIRE(globals.size() == 2);
    CHECK(globals[0].name == "n");
    CHECK(std::get<double>(globals[0].value) == 2.5);
    CHECK(globals[1].name == "s");
    CHECK(std::get<std::string>(globals[1].value) == "x");
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

TEST_CASE("update_vars_from_globals writes VM values back into SaveVar") {
    gl::script::ScriptGlobal i{"int", "i", std::int32_t{7}};
    gl::script::ScriptGlobal d{"DWORD", "d", std::uint32_t{9}};
    gl::script::ScriptGlobal f{"float", "f", 2.5};
    gl::script::ScriptGlobal s{"string", "s", std::string{"x"}};
    const std::vector<gl::script::ScriptGlobal> initial{i, d, f, s};
    NullHost host;
    gl::script::ScriptVm vm({}, host);
    vm.set_globals(initial);

    gl::assets::SaveVar vi;
    vi.type = 0;
    vi.name = "i";
    gl::assets::SaveVar vd = vi;
    vd.type = 1;
    vd.name = "d";
    gl::assets::SaveVar vf = vi;
    vf.type = 2;
    vf.name = "f";
    gl::assets::SaveVar vs;
    vs.type = 3;
    vs.name = "s";
    gl::assets::SaveVar missing;
    missing.type = 0;
    missing.name = "missing";
    const double keep = 1.5;
    std::memcpy(missing.value.data(), &keep, 8);

    const auto updated = gl::script::update_vars_from_globals(
        {vi, vd, vf, vs, missing}, vm);
    REQUIRE(updated.size() == 5);
    CHECK(gl::assets::save_var_number(updated[0]).has_value());
    CHECK(*gl::assets::save_var_number(updated[0]) == 7.0);
    CHECK(*gl::assets::save_var_number(updated[1]) == 9.0);
    CHECK(*gl::assets::save_var_number(updated[2]) == 2.5);
    CHECK(updated[3].text_value == "x");
    CHECK(*gl::assets::save_var_number(updated[4]) == 1.5);
    CHECK(updated[0].type == 0);
    CHECK(updated[1].name == "d");
}
