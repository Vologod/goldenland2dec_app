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
#include <span>
#include <string>
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

std::vector<std::uint8_t> block_body(const gl::assets::SavePayload &payload,
                                     std::size_t index) {
    const auto &block = payload.blocks[index];
    return std::vector<std::uint8_t>(
        payload.decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset),
        payload.decompressed.begin() +
            static_cast<std::ptrdiff_t>(block.offset + block.size));
}
} // namespace

TEST_CASE("real data: BT_VARS re-encodes byte-identically" *
          doctest::skip(!game_dir_available())) {
    auto vfs = gl::assets::GameVfs::mount(std::getenv("GL_GAME_DIR"));
    REQUIRE(vfs.has_value());
    auto globals_data = vfs->read("scripts/dialogs_special/zlato_vars.scr");
    REQUIRE(globals_data.has_value());
    auto parsed_globals = gl::script::parse_globals(globals_data.value());
    REQUIRE(parsed_globals.has_value());

    for (int slot = 0; slot < 28; ++slot) {
        auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        CAPTURE(slot);

        NullHost host;
        gl::script::ScriptVm vm({}, host);
        vm.set_globals(parsed_globals.value());
        vm.apply_saved_globals(gl::script::save_globals_from_vars(payload->vars));

        const auto updated =
            gl::script::update_vars_from_globals(payload->vars, vm);
        const auto body = gl::assets::encode_vars_body(updated);
        REQUIRE(body.has_value());
        CHECK(body.value() == block_body(payload.value(), 2));
    }
}
