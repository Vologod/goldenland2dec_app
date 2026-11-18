#include "gl/assets/hero_save_body.hpp"
#include "gl/assets/save_payload.hpp"

#include <array>
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

std::vector<std::uint8_t> hero_body(const gl::assets::SavePayload &payload) {
    const auto &block = payload.blocks[6];
    return std::vector<std::uint8_t>(
        payload.decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset),
        payload.decompressed.begin() + static_cast<std::ptrdiff_t>(block.offset + block.size));
}
} // namespace

TEST_CASE("hero save body real: 28 bodies round-trip" * doctest::skip(!game_dir_available())) {
    std::size_t items = 0;
    std::size_t mods = 0;
    for (int slot = 0; slot < 28; ++slot) {
        CAPTURE(slot);
        const auto payload = gl::assets::parse_save_payload(read_save(slot));
        REQUIRE(payload.has_value());
        const auto body = hero_body(payload.value());
        const auto parsed = gl::assets::parse_hero_save_body(body);
        REQUIRE(parsed.has_value());
        const auto encoded = gl::assets::encode_hero_save_body(parsed.value());
        REQUIRE(encoded.has_value());
        CHECK(encoded.value() == body);
        for (const auto &container : parsed->containers) {
            CHECK(container.flag == 1);
            items += container.items.size();
            for (const auto &item : container.items)
                mods += item.modifiers.size();
        }
    }
    CHECK(items == 1599);
    CHECK(mods == 924);
}

TEST_CASE("hero save body real: pinned slots" * doctest::skip(!game_dir_available())) {
    struct Pin {
        int slot;
        std::uint32_t tile_x, tile_y, direction;
        std::uint32_t hp, ep, ap;
        std::array<std::size_t, 3> containers;
    };
    const std::array<Pin, 3> pins{{
        {0, 123, 131, 1, 263, 0, 5, {8, 117, 11}},
        {2, 103, 133, 1, 201, 0, 51, {9, 140, 11}},
        {21, 187, 158, 3, 34, 33, 26, {0, 2, 0}},
    }};
    for (const Pin &pin : pins) {
        CAPTURE(pin.slot);
        const auto payload = gl::assets::parse_save_payload(read_save(pin.slot));
        REQUIRE(payload.has_value());
        const auto parsed = gl::assets::parse_hero_save_body(hero_body(payload.value()));
        REQUIRE(parsed.has_value());
        CHECK(parsed->tile_x == pin.tile_x);
        CHECK(parsed->tile_y == pin.tile_y);
        CHECK(parsed->direction == pin.direction);
        CHECK(parsed->hp == pin.hp);
        CHECK(parsed->ep == pin.ep);
        CHECK(parsed->ap == pin.ap);
        for (std::size_t index = 0; index < pin.containers.size(); ++index)
            CHECK(parsed->containers[index].items.size() == pin.containers[index]);
    }
}
