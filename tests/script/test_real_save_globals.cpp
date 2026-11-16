#include "gl/assets/save_payload.hpp"
#include "gl/assets/vfs.hpp"
#include "gl/script/globals.hpp"
#include "gl/script/script_vm.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

struct NullHost : gl::script::ScriptHost {
    gl::script::Value call(std::uint32_t, std::span<const gl::script::Value>) override {
        return std::int32_t{0};
    }
};

gl::assets::Bytes read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) /
                                       "Saves" / ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return gl::assets::Bytes(std::istreambuf_iterator<char>(file), {});
}

std::vector<gl::script::ScriptGlobal>
declared_globals(gl::assets::GameVfs &vfs) {
    auto data = vfs.read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(data.has_value());
    auto parsed = gl::script::parse_globals(data.value());
    REQUIRE(parsed.has_value());
    return parsed.value();
}
} // namespace

TEST_CASE("real data: saved globals apply to the dialog vm" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto declared_list = declared_globals(vfs.value());
    std::map<std::string, gl::script::Value> declared;
    for (const gl::script::ScriptGlobal &global : declared_list)
        declared[global.name] = global.value;

    std::size_t total_vars = 0;
    std::size_t type0 = 0, type1 = 0, type2 = 0, type3 = 0;
    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);
        total_vars += payload->vars.size();
        for (const auto &var : payload->vars) {
            if (var.type == 0) ++type0;
            if (var.type == 1) ++type1;
            if (var.type == 2) ++type2;
            if (var.type == 3) ++type3;
        }

        NullHost host;
        gl::script::ScriptVm vm({}, host);
        vm.set_globals(declared_list);
        const auto saved = gl::script::save_globals_from_vars(payload->vars);
        const auto counts = vm.apply_saved_globals(saved);
        CHECK(counts.matched + counts.added == payload->vars.size());
        CHECK(counts.kept == 0);

        for (const auto &var : payload->vars) {
            const auto found = vm.globals().find(var.name);
            REQUIRE(found != vm.globals().end());
            if (var.type == 3) {
                CHECK(std::get<std::string>(found->second) == var.text_value);
                continue;
            }
            const auto number = gl::assets::save_var_number(var);
            REQUIRE(number.has_value());
            const auto declaration = declared.find(var.name);
            if (declaration == declared.end()) {
                CHECK(std::get<double>(found->second) == *number);
            } else if (std::holds_alternative<std::int32_t>(declaration->second)) {
                CHECK(std::get<std::int32_t>(found->second) ==
                      static_cast<std::int32_t>(*number));
            } else if (std::holds_alternative<std::uint32_t>(declaration->second)) {
                CHECK(std::get<std::uint32_t>(found->second) ==
                      static_cast<std::uint32_t>(*number));
            } else {
                CHECK(std::get<double>(found->second) == *number);
            }
        }
    }
    CHECK(total_vars == 26191);
    CHECK(type0 == 23498);
    CHECK(type1 == 2632);
    CHECK(type2 == 61);
    CHECK(type3 == 0);
}

TEST_CASE("real data: saved global pins" * doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    const auto declared_list = declared_globals(vfs.value());

    struct Pin {
        int slot;
        std::size_t count;
        const char *name;
        double value;
    };
    const std::vector<Pin> pins{
        {0, 1002, "voevoda_povtor", 10.0},
        {0, 1002, "ShurbaHal_guardian_povtor", 3.0},
        {2, 1004, "L2_male1_povtor", 1.0},
        {21, 879, "voevoda_povtor", 0.0},
    };
    for (const Pin &pin : pins) {
        auto payload = gl::assets::parse_save_payload(read_save(pin.slot));
        REQUIRE(payload.has_value());
        CAPTURE(pin.slot);
        CAPTURE(pin.name);
        CHECK(payload->vars.size() == pin.count);
        NullHost host;
        gl::script::ScriptVm vm({}, host);
        vm.set_globals(declared_list);
        const auto saved = gl::script::save_globals_from_vars(payload->vars);
        vm.apply_saved_globals(saved);
        const auto found = vm.globals().find(pin.name);
        REQUIRE(found != vm.globals().end());
        CHECK(std::get<std::int32_t>(found->second) ==
              static_cast<std::int32_t>(pin.value));
    }
}
