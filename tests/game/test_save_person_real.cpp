#include "gl/assets/save_payload.hpp"
#include "gl/game/save_person.hpp"

#include <cstddef>
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

TEST_CASE("real data: all type-2 bodies round-trip" * doctest::skip(!game_dir_available())) {
    std::size_t records = 0;
    std::size_t selected = 0;
    for (int slot = 0; slot < 28; ++slot) {
        CAPTURE(slot);
        const auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        for (std::size_t section = 0; section < payload->tail.size(); ++section) {
            for (const auto &entity : payload->tail[section].entities) {
                if (entity.type != 2)
                    continue;
                const auto decoded = gl::game::decode_save_person(entity);
                REQUIRE(decoded.has_value());
                const auto encoded = gl::game::encode_save_person_body(decoded.value());
                REQUIRE(encoded.has_value());
                CHECK(encoded.value() == entity.body);
                ++records;
                if (section == *payload->level_tail_index)
                    ++selected;
            }
        }
    }
    CHECK(records == 14507);
    CHECK(selected == 499);
}
