#include "gl/assets/save_payload.hpp"
#include "gl/game/diary.hpp"

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

TEST_CASE("real data: diary headers decode from 28 saves" *
          doctest::skip(!game_dir_available())) {
    std::size_t entries = 0;
    std::size_t ends = 0;
    for (int slot = 0; slot < 28; ++slot) {
        CAPTURE(slot);
        const auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        const auto decoded = gl::game::decode_diary_entries(payload.value());
        REQUIRE(decoded.has_value());
        CHECK(decoded->size() == payload->diary.size());
        for (std::size_t index = 0; index < decoded->size(); ++index) {
            CHECK(decoded.value()[index].key <= 550);
            CHECK(decoded.value()[index].status <= 1);
            CHECK(decoded.value()[index].key == payload->diary[index].key);
            CHECK(decoded.value()[index].record_count ==
                  payload->diary[index].blocks.size() - 1);
            CHECK(decoded.value()[index].blocks == payload->diary[index].blocks);
            CHECK(decoded.value()[index].text_id >= 180);
            CHECK(decoded.value()[index].text_id <= 510);
            if (decoded.value()[index].end.has_value()) {
                ++ends;
                CHECK(decoded.value()[index].end_time_flag == 1);
            } else {
                CHECK(decoded.value()[index].end_time_flag == 0);
            }
            ++entries;
        }
    }
    CHECK(entries == 1013);
    CHECK(ends == 797);
}
