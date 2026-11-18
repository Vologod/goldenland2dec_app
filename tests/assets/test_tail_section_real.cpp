#include "gl/assets/save_payload.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
bool game_dir_available() { return std::getenv("GL_GAME_DIR") != nullptr; }

std::vector<std::uint8_t> read_save(int slot) {
    const std::filesystem::path path = std::filesystem::path(std::getenv("GL_GAME_DIR")) / "Saves" /
                                       ("slot" + std::to_string(slot) + ".sav");
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

TEST_CASE("real data: every tail section re-encodes byte-identically" *
          doctest::skip(!game_dir_available())) {
    std::size_t sections = 0;
    for (int slot = 0; slot < 28; ++slot) {
        CAPTURE(slot);
        const auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        for (const auto &entry : payload->tail) {
            const auto encoded =
                gl::assets::encode_tail_section(entry.rginf, entry.person_names, entry.entities);
            REQUIRE(encoded.has_value());
            CHECK(encoded.value() == entry.section);
            ++sections;
        }
    }
    CHECK(sections == 2599);
}
