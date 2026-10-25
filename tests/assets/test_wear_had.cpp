#include "gl/assets/wear_had.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace {
void put_u32(std::vector<std::uint8_t> &d, std::size_t o, std::uint32_t v) {
    std::memcpy(d.data() + o, &v, sizeof(v));
}
void put_f32(std::vector<std::uint8_t> &d, std::size_t o, float v) {
    std::memcpy(d.data() + o, &v, sizeof(v));
}
} // namespace

TEST_CASE("parse_wear_had reads one animation record") {
    std::vector<std::uint8_t> data(0x6C, 0);
    std::memcpy(data.data(), "HAD ", 4);
    put_u32(data, 4, static_cast<std::uint32_t>(data.size()));
    put_u32(data, 8, 0x4);
    std::size_t o = 0xC;
    put_u32(data, o + 0, 0x4);
    put_u32(data, o + 4, 92);
    put_u32(data, o + 8, 88);
    put_u32(data, o + 12, 12);
    put_u32(data, o + 16, 158);
    put_u32(data, o + 20, 142);
    put_u32(data, o + 24, 48);
    put_u32(data, o + 28, 92);
    put_u32(data, o + 32, 24);
    put_u32(data, o + 36, 77);
    put_f32(data, o + 40, 5.5f);
    put_f32(data, o + 44, 4.0f);
    put_u32(data, o + 48, 8);
    put_u32(data, o + 60, 148);
    put_u32(data, o + 64, 202);
    put_u32(data, o + 68, 55);
    put_u32(data, o + 72, 32);
    put_u32(data, o + 76, 34);
    put_u32(data, o + 80, 17);
    put_u32(data, o + 84, 8);

    auto parsed = gl::assets::parse_wear_had(data);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->size() == 1);
    const auto &a = parsed->front();
    CHECK(a.type_mask == 0x4);
    CHECK(a.rows == 5);
    CHECK(a.delay_ms == 88);
    CHECK(a.columns == 12);
    CHECK(a.frame_width == 48);
    CHECK(a.frame_height == 92);
    CHECK(a.anchor_x == 24);
    CHECK(a.anchor_y == 77);
    CHECK(a.movement_x == doctest::Approx(5.5f));
    CHECK(a.movement_y == doctest::Approx(4.0f));
    CHECK(a.layer_offset_x == 158);
    CHECK(a.layer_offset_y == 142);
    CHECK(a.shadow_offset_x == 148);
    CHECK(a.shadow_offset_y == 202);
    CHECK(a.shadow_width == 55);
    CHECK(a.shadow_height == 32);
    CHECK(a.shadow_anchor_x == 34);
    CHECK(a.shadow_anchor_y == 17);
}

TEST_CASE("parse_wear_had marks 9-row animations") {
    std::vector<std::uint8_t> data(0x6C, 0);
    std::memcpy(data.data(), "HAD ", 4);
    put_u32(data, 4, static_cast<std::uint32_t>(data.size()));
    std::size_t o = 0xC;
    put_u32(data, o + 0, 0x10);
    put_u32(data, o + 4, 92);
    put_u32(data, o + 8, 82);
    put_u32(data, o + 12, 12);
    put_u32(data, o + 16, 154);
    put_u32(data, o + 20, 140);
    put_u32(data, o + 24, 65);
    put_u32(data, o + 28, 95);
    put_u32(data, o + 32, 28);
    put_u32(data, o + 36, 79);
    put_u32(data, o + 48, 8);
    put_u32(data, o + 84, 8);
    auto parsed = gl::assets::parse_wear_had(data);
    REQUIRE(parsed.has_value());
    CHECK(parsed->front().rows == 9);
}

TEST_CASE("parse_wear_had rejects bad input") {
    std::vector<std::uint8_t> data(0x20, 0);
    CHECK_FALSE(gl::assets::parse_wear_had(data).has_value());
    std::memcpy(data.data(), "HAD ", 4);
    put_u32(data, 4, 0x20);
    put_u32(data, 8, 0x4);
    put_u32(data, 0xC, 0x4);
    put_u32(data, 0x10, 0x1000);
    CHECK_FALSE(gl::assets::parse_wear_had(data).has_value());
}
